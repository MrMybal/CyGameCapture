// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — D3D12 Spout output through a D3D11On12 bridge.
//
// Why a bridge: every Spout receiver (OBS Spout2 plugin, Resolume, CyGameCaptureOBS, ...) opens the
// sender's texture with ID3D11Device::OpenSharedResource, which only accepts *legacy* DXGI shared
// handles. D3D12 can only produce NT handles, so a D3D12 resource can never be published directly.
// A D3D11On12 device created on the game's own ID3D12Device + presenting queue can create a classic
// D3D11 shared texture and copy into it from a D3D12 resource wrapped as a D3D11 resource.
//
// Ordering (why no fence is needed here, unlike the Unreal plugin):
//   ReShade's D3D12 command_queue::flush_immediate_command_list() calls ExecuteCommandLists
//   synchronously on the calling thread. At present time the sequence is therefore, on one thread:
//     1. record  copy(source -> intermediate)   on ReShade's immediate command list
//     2. flush_immediate_command_list()         -> the copy is submitted to the game queue
//     3. bridge: Acquire / CopyResource(shared <- wrapped intermediate) / Release / Flush
//        -> the D3D11On12 context submits to that *same* queue, so the GPU runs 1 then 3 in order.
//   Cost: 2 GPU copies per frame (game copy + bridge copy), no CPU stall, no fence wait.
#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <cstdint>
#include <mutex>
#include <string>

namespace cygc
{
	/**
	 * Process-wide D3D11On12 device built on the game's D3D12 device and presenting queue.
	 * Created lazily by the first D3D12 capture stream, released when the add-on unloads.
	 * The D3D11 immediate context of the bridge is single threaded: every use takes Lock().
	 */
	class D3D11On12Bridge
	{
	public:
		static D3D11On12Bridge &Get();

		/** Builds the bridge from the game's native D3D12 device / queue (from reshade get_native()). */
		bool Initialize(ID3D12Device *device12, ID3D12CommandQueue *queue, std::string &error);
		void Shutdown();

		/**
		 * Releases the bridge only when it was built on this D3D12 device. Called from the 'destroy_device'
		 * event: after that point the game's device and queue are gone and flushing the 11on12 context would
		 * dereference freed objects.
		 */
		void ShutdownForDevice(ID3D12Device *device12);
		bool IsReady() const { return device11_ != nullptr && on12_ != nullptr; }

		/** The D3D12 device the bridge was built on (streams verify they all share it). */
		ID3D12Device *Device12() const { return device12_; }

		/** Creates a D3D11 shared texture (legacy handle) on the bridge device. */
		bool CreateSharedTexture(uint32_t width, uint32_t height, uint32_t dxgi_format, ID3D11Texture2D **out_texture, HANDLE *out_handle, std::string &error);

		/** Wraps a D3D12 resource so the bridge's D3D11 context can read it. */
		bool WrapResource(ID3D12Resource *resource12, D3D12_RESOURCE_STATES state, ID3D11Resource **out_wrapped, std::string &error);

		/** Acquire -> CopyResource(dest <- wrapped source) -> Release -> Flush. Call under Lock(). */
		void CopyWrappedToShared(ID3D11Resource *wrapped_source, ID3D11Texture2D *shared_dest);

		std::mutex &Lock() { return mutex_; }

	private:
		D3D11On12Bridge() = default;
		~D3D11On12Bridge();

		std::mutex mutex_;
		ID3D12Device *device12_ = nullptr;          // not owned (the game owns it)
		ID3D12CommandQueue *queue_ = nullptr;       // not owned
		ID3D11Device *device11_ = nullptr;
		ID3D11DeviceContext *context11_ = nullptr;
		ID3D11On12Device *on12_ = nullptr;
	};
}
