// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — thin wrapper over the Spout2 SDK sender registration.
//
// CyGameCaptureRS does *not* use spoutDX::SendTexture: that path would need its own copy
// into a Spout-owned texture. Instead the persistent output texture is created through the
// ReShade API with the "shared" flag (D3D11: D3D11_RESOURCE_MISC_SHARED, legacy DXGI handle),
// and only the sender bookkeeping is delegated to Spout:
//   * spoutSenderNames  : registers "name -> (width, height, share handle, DXGI format)" in the
//                         shared memory map every Spout receiver (OBS Spout2 plugin, ...) reads
//   * spoutFrameCount   : named access mutex around texture updates + optional frame counter/FPS
#pragma once

#include <Windows.h>
#include <cstdint>
#include <memory>
#include <string>

class spoutSenderNames;
class spoutFrameCount;

namespace cygc
{
	class SpoutSender
	{
	public:
		SpoutSender();
		~SpoutSender();

		SpoutSender(const SpoutSender &) = delete;
		SpoutSender &operator=(const SpoutSender &) = delete;

		// Stream 1: "CyGameCaptureRS::Game", stream N: "CyGameCaptureRS::Game::N".
		// When a candidate is already taken (another process / another stream) the index is incremented.
		static std::string MakeUniqueName(const std::string &base_name, unsigned int first_index = 1);

		// Spout keeps every sender name of the machine in one fixed-size shared memory table. Its size is
		// "MaxSenders" (64 by default, HKCU\Software\Leading Edge\Spout\MaxSenders) and is shared by every
		// Spout application. When the table is full Spout refuses new names *silently*, so check first.
		static int MaxSenders();
		static int ActiveSenderCount();
		static bool CanRegisterNewSender(std::string *reason = nullptr);

		bool Create(const std::string &name, uint32_t width, uint32_t height, HANDLE share_handle, uint32_t dxgi_format);
		bool Update(uint32_t width, uint32_t height, HANDLE share_handle, uint32_t dxgi_format);
		void Release();

		// Wrap every GPU update of the shared texture between BeginFrame/EndFrame.
		// BeginFrame returns false when a receiver holds the access mutex for too long (frame is skipped).
		bool BeginFrame();
		void EndFrame();

		bool IsCreated() const { return created_; }
		const std::string &Name() const { return name_; }
		uint32_t Width() const { return width_; }
		uint32_t Height() const { return height_; }
		uint32_t DxgiFormat() const { return format_; }

	private:
		std::unique_ptr<spoutSenderNames> names_;
		std::unique_ptr<spoutFrameCount> frame_;
		std::string name_;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		uint32_t format_ = 0;
		bool created_ = false;
	};
}
