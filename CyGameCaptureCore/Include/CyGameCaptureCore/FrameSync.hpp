// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureCore — cross-process frame index channel.
//
// Why this exists
// ---------------
// Spout carries a texture and nothing else. Two senders updated in the same game frame are, from a
// receiver's point of view, two completely unrelated streams: there is no way to tell whether the
// picture currently in sender A and the one in sender B belong to the same frame. That is harmless for
// a live preview and unacceptable for recording, where "the clean scene" and "the HUD" (or "the colour"
// and "the depth") must line up frame for frame in the resulting files.
//
// So the producers (CyGameCaptureRS, later CyGameCaptureUE) publish, next to their Spout senders, one
// small shared-memory record per stream holding the index of the frame that was last copied into the
// shared texture, plus the group all streams of the same producer frame belong to. A recorder reads
// those records and only commits a frame once every stream it records reports the same index.
//
// This channel never carries pixels. It is a few dozen bytes per stream per frame.
#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cygc
{
	inline constexpr char kFrameSyncMappingName[] = "Local\\CyGameCapture_FrameSync_v1";
	inline constexpr uint32_t kFrameSyncMagic = 0x43594753;   // 'CYGS'
	inline constexpr uint32_t kFrameSyncVersion = 1;

	/** Same ceiling as Spout's default sender table; a slot is ~320 bytes, so the block stays tiny. */
	inline constexpr uint32_t kFrameSyncSlotCount = 256;
	inline constexpr uint32_t kFrameSyncNameLength = 256;

	/**
	 * One stream. Written by the producer, read by any number of recorders.
	 *
	 * `sequence` is a seqlock: the writer makes it odd before touching the fields and even again after,
	 * so a reader that sees the same even value before and after its copy knows the copy is consistent.
	 * Nothing here needs a kernel object, which matters because this is written once per frame from the
	 * present callback of a game.
	 */
	struct FrameSyncSlot
	{
		std::atomic<uint32_t> sequence;     // 0 = free, odd = being written, even = readable
		std::atomic<uint32_t> owner_pid;    // 0 when the slot is free; used to reclaim slots of dead processes

		char sender_name[kFrameSyncNameLength];

		uint64_t frame_index;               // producer frame counter, shared by every stream of that frame
		uint64_t group_id;                  // identifies the producer (process + device), so streams can be grouped
		uint64_t qpc_timestamp;             // QueryPerformanceCounter when the copy was submitted

		uint32_t width;
		uint32_t height;
		uint32_t dxgi_format;
		uint32_t stream_kind;               // cygc::StreamKind
	};

	struct FrameSyncHeader
	{
		uint32_t magic;
		uint32_t version;
		uint32_t slot_count;
		uint32_t reserved;
	};

	struct FrameSyncBlock
	{
		FrameSyncHeader header;
		FrameSyncSlot slots[kFrameSyncSlotCount];
	};

	/** Snapshot of one stream, as a reader sees it. */
	struct FrameSyncRecord
	{
		std::string sender_name;
		uint64_t frame_index = 0;
		uint64_t group_id = 0;
		uint64_t qpc_timestamp = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t dxgi_format = 0;
		uint32_t stream_kind = 0;
		uint32_t owner_pid = 0;
	};

	namespace detail
	{
		/**
		 * Maps the block, creating it if needed. The mapping is process-wide and lives until the process
		 * exits: producers and recorders come and go, the block does not.
		 */
		inline FrameSyncBlock *MapFrameSyncBlock()
		{
			static FrameSyncBlock *block = []() -> FrameSyncBlock * {
				const HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
					static_cast<DWORD>(sizeof(FrameSyncBlock)), kFrameSyncMappingName);
				if (mapping == nullptr)
					return nullptr;

				const bool created = GetLastError() != ERROR_ALREADY_EXISTS;
				auto *const mapped = static_cast<FrameSyncBlock *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FrameSyncBlock)));
				if (mapped == nullptr)
				{
					CloseHandle(mapping);
					return nullptr;
				}

				// The section is zero filled by the kernel on creation, so only the header needs writing.
				if (created)
				{
					mapped->header.magic = kFrameSyncMagic;
					mapped->header.version = kFrameSyncVersion;
					mapped->header.slot_count = kFrameSyncSlotCount;
				}
				// The mapping handle is deliberately leaked: closing it would drop the section as soon as
				// the last process unmapped it, and the view stays valid for the lifetime of the process.
				return mapped;
			}();
			return block;
		}

		/** A slot whose owning process is gone is stale and can be reused. */
		inline bool IsOwnerAlive(uint32_t pid)
		{
			if (pid == 0)
				return false;
			if (pid == GetCurrentProcessId())
				return true;
			const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
			if (process == nullptr)
				return false;
			const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
			CloseHandle(process);
			return alive;
		}
	}

	/**
	 * Producer side. One instance per stream, claimed with the Spout sender name so a recorder can match
	 * the two without any other agreement.
	 */
	class FrameSyncWriter
	{
	public:
		FrameSyncWriter() = default;
		~FrameSyncWriter() { Release(); }

		FrameSyncWriter(const FrameSyncWriter &) = delete;
		FrameSyncWriter &operator=(const FrameSyncWriter &) = delete;

		bool Claim(const std::string &sender_name, uint64_t group_id)
		{
			Release();
			FrameSyncBlock *const block = detail::MapFrameSyncBlock();
			if (block == nullptr || sender_name.empty())
				return false;

			for (uint32_t i = 0; i < kFrameSyncSlotCount; ++i)
			{
				FrameSyncSlot &slot = block->slots[i];

				// Free, or left behind by a process that died without releasing it
				const uint32_t owner = slot.owner_pid.load(std::memory_order_acquire);
				const bool reusable = owner == 0 || (owner != GetCurrentProcessId() && !detail::IsOwnerAlive(owner));
				if (!reusable)
					continue;

				uint32_t expected = owner;
				if (!slot.owner_pid.compare_exchange_strong(expected, GetCurrentProcessId(), std::memory_order_acq_rel))
					continue;

				slot.sequence.store(1, std::memory_order_release);   // odd: being written
				std::memset(slot.sender_name, 0, sizeof(slot.sender_name));
				strncpy_s(slot.sender_name, sender_name.c_str(), _TRUNCATE);
				slot.frame_index = 0;
				slot.group_id = group_id;
				slot.qpc_timestamp = 0;
				slot.width = 0;
				slot.height = 0;
				slot.dxgi_format = 0;
				slot.stream_kind = 0;
				slot.sequence.store(2, std::memory_order_release);   // even: readable

				slot_ = &slot;
				group_id_ = group_id;
				return true;
			}
			return false;
		}

		void Release()
		{
			if (slot_ == nullptr)
				return;
			slot_->sequence.store(0, std::memory_order_release);
			slot_->owner_pid.store(0, std::memory_order_release);
			slot_ = nullptr;
		}

		bool IsClaimed() const { return slot_ != nullptr; }

		/** Called right after the copy for this stream has been submitted, once per frame. */
		void Publish(uint64_t frame_index, uint32_t width, uint32_t height, uint32_t dxgi_format, uint32_t stream_kind)
		{
			if (slot_ == nullptr)
				return;

			LARGE_INTEGER now = {};
			QueryPerformanceCounter(&now);

			const uint32_t sequence = slot_->sequence.load(std::memory_order_relaxed);
			slot_->sequence.store(sequence | 1u, std::memory_order_release);   // odd while writing

			slot_->frame_index = frame_index;
			slot_->group_id = group_id_;
			slot_->qpc_timestamp = static_cast<uint64_t>(now.QuadPart);
			slot_->width = width;
			slot_->height = height;
			slot_->dxgi_format = dxgi_format;
			slot_->stream_kind = stream_kind;

			slot_->sequence.store((sequence | 1u) + 1u, std::memory_order_release);   // even again
		}

	private:
		FrameSyncSlot *slot_ = nullptr;
		uint64_t group_id_ = 0;
	};

	/** Reader side: one snapshot of every live stream. Cheap enough to call once per recorded frame. */
	inline std::vector<FrameSyncRecord> ReadFrameSync()
	{
		std::vector<FrameSyncRecord> result;
		FrameSyncBlock *const block = detail::MapFrameSyncBlock();
		if (block == nullptr || block->header.magic != kFrameSyncMagic || block->header.version != kFrameSyncVersion)
			return result;

		for (uint32_t i = 0; i < kFrameSyncSlotCount; ++i)
		{
			FrameSyncSlot &slot = block->slots[i];

			const uint32_t before = slot.sequence.load(std::memory_order_acquire);
			if (before == 0 || (before & 1u) != 0)
				continue;   // free, or a write is in progress: skip it this time

			FrameSyncRecord record;
			char name[kFrameSyncNameLength];
			std::memcpy(name, slot.sender_name, sizeof(name));
			name[kFrameSyncNameLength - 1] = '\0';
			record.frame_index = slot.frame_index;
			record.group_id = slot.group_id;
			record.qpc_timestamp = slot.qpc_timestamp;
			record.width = slot.width;
			record.height = slot.height;
			record.dxgi_format = slot.dxgi_format;
			record.stream_kind = slot.stream_kind;
			record.owner_pid = slot.owner_pid.load(std::memory_order_acquire);

			// Torn read: the producer wrote during the copy, drop this sample
			if (slot.sequence.load(std::memory_order_acquire) != before)
				continue;

			record.sender_name = name;
			if (!record.sender_name.empty())
				result.push_back(std::move(record));
		}
		return result;
	}

	/** Snapshot of one stream by Spout sender name. */
	inline bool ReadFrameSync(const std::string &sender_name, FrameSyncRecord &out)
	{
		for (FrameSyncRecord &record : ReadFrameSync())
		{
			if (record.sender_name == sender_name)
			{
				out = std::move(record);
				return true;
			}
		}
		return false;
	}
}
