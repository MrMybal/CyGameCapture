// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — per-graphics-API capability description.
//
// Resource tracking, the buffer inspector and the GPU preview are written purely
// against reshade::api and therefore work on every API ReShade supports.
// The Spout output, however, needs a texture whose *legacy* DXGI shared handle can be
// opened by receivers (OBS Spout2 plugin uses ID3D11Device::OpenSharedResource):
//
//   D3D10 / D3D11 : ReShade creates the persistent texture with D3D11_RESOURCE_MISC_SHARED
//                   -> legacy handle -> Spout compatible. No native code needed.
//   D3D12         : ReShade can only create NT-handle shared resources; those cannot be opened by
//                   legacy receivers, so the output goes through a D3D11On12 bridge that owns the
//                   real shared texture (see Backends/D3D12/D3D11On12Bridge.hpp). Costs one extra copy.
//   OpenGL/Vulkan : Out of scope for now.
#pragma once

#include <reshade_api_device.hpp>
#include <string>

namespace cygc
{
	struct BackendSupport
	{
		reshade::api::device_api api = reshade::api::device_api::d3d11;
		const char *api_name = "";
		bool tracking = true;        // resource tracking + inspector
		bool preview = true;         // GPU preview inside the overlay
		bool spout_capture = false;  // persistent shared texture + Spout sender
		std::string reason;          // why spout_capture is not available (empty when it is)
	};

	const char *DeviceApiName(reshade::api::device_api api);
	BackendSupport QueryBackendSupport(reshade::api::device *device);
}
