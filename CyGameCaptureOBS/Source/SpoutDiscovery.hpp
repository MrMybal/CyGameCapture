// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureOBS — Spout2 sender discovery and per-sender shared-memory description.
//
// Only the CPU side of Spout is used here: the machine-wide sender name table, the per-sender
// information block (share handle, size, DXGI format, owner executable) and the named access mutex.
// The picture itself never travels through this file: OBS opens the shared texture directly on the
// GPU with gs_texture_open_shared().
#pragma once

#include <Windows.h>

#include <string>
#include <vector>

namespace cygc::obs
{
	/** What Spout publishes about one sender. */
	struct SenderDescription
	{
		std::string name;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t dxgi_format = 0;
		HANDLE share_handle = nullptr;

		bool IsValid() const { return width != 0 && height != 0 && share_handle != nullptr; }
		bool SameTexture(const SenderDescription &other) const
		{
			return share_handle == other.share_handle && width == other.width && height == other.height && dxgi_format == other.dxgi_format;
		}
	};

	/** Every sender currently registered on the machine, sorted by name. */
	std::vector<SenderDescription> EnumerateSenders();

	/** Live lookup of one sender; false when it is not registered (any more). */
	bool GetSenderDescription(const std::string &name, SenderDescription &out);

	/** Name of Spout's "active sender", the one other Spout applications default to. */
	bool GetActiveSenderName(std::string &out);

	/** Size of Spout's machine-wide name table and how much of it is in use. */
	int MaxSenders();
	int ActiveSenderCount();

	/**
	 * Holds the named access mutex of one sender ("<name>_SpoutAccessMutex") and its frame counter.
	 * Receivers take the mutex around the frame they read, exactly like Spout's own receivers, so a
	 * sender does not swap the texture description underneath them.
	 */
	class SenderAccess
	{
	public:
		SenderAccess();
		~SenderAccess();

		SenderAccess(const SenderAccess &) = delete;
		SenderAccess &operator=(const SenderAccess &) = delete;

		/** Attach to the mutex and frame counter of an existing sender. */
		bool Open(const std::string &name);
		void Close();
		bool IsOpen() const { return open_; }
		const std::string &Name() const { return name_; }

		/** Spout waits up to 67 ms; false means a peer held it too long, skip this frame. */
		bool BeginAccess();
		void EndAccess();

		/** True when the sender produced a new frame since the last call (needs EndAccess afterwards). */
		bool HasNewFrame();

		/** Frame rate the sender is publishing, 0 when it does not publish one. */
		double SenderFps() const;

	private:
		struct Impl;
		Impl *impl_ = nullptr;
		std::string name_;
		bool open_ = false;
	};
}
