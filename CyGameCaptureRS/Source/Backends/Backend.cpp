// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Backend.hpp"

using namespace reshade::api;

namespace cygc
{
	const char *DeviceApiName(device_api api)
	{
		switch (api)
		{
		case device_api::d3d9: return "D3D9";
		case device_api::d3d10: return "D3D10";
		case device_api::d3d11: return "D3D11";
		case device_api::d3d12: return "D3D12";
		case device_api::opengl: return "OpenGL";
		case device_api::vulkan: return "Vulkan";
		}
		return "Unknown";
	}

	BackendSupport QueryBackendSupport(device *device)
	{
		BackendSupport support;
		support.api = device->get_api();
		support.api_name = DeviceApiName(support.api);

		switch (support.api)
		{
		case device_api::d3d10:
		case device_api::d3d11:
			// ReShade exposes shared resource creation; legacy DXGI handle == what Spout receivers open.
			support.spout_capture = device->check_capability(device_caps::shared_resource);
			if (!support.spout_capture)
				support.reason = "Device reports no shared resource support";
			break;
		case device_api::d3d12:
			// Spout output goes through the D3D11On12 bridge (Backends/D3D12/D3D11On12Bridge.hpp): a D3D12
			// resource can never carry a legacy DXGI shared handle, which is all Spout receivers can open.
			support.spout_capture = true;
			break;
		case device_api::d3d9:
			support.spout_capture = false;
			support.reason = "D3D9 is not supported for Spout output.";
			break;
		case device_api::opengl:
		case device_api::vulkan:
			support.spout_capture = false;
			support.reason = "OpenGL / Vulkan output is out of scope for now.";
			break;
		}
		return support;
	}
}
