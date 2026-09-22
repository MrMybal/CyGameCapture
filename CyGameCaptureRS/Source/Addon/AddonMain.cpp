// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — ReShade add-on entry points.
//
// ReShade (add-on enabled build, 6.8.0 / API 20) loads every *.addon64 found next to the
// game executable (or in [ADDON] AddonPath), calls AddonInit, then dispatches the events
// registered below. NAME / DESCRIPTION are shown in the ReShade "Add-ons" tab.
#include <imgui.h>      // must come before reshade.hpp: enables the ImGui function table in register_addon
#include <reshade.hpp>

#include "Config.hpp"
#include "DeviceContext.hpp"
#include "Log.hpp"
#include "ResourceTracker/ResourceTracker.hpp"
#include "Backends/D3D12/D3D11On12Bridge.hpp"
#include "Assistant/AiAssistant.hpp"
#include "UI/Logo.hpp"
#include "UI/Overlay.hpp"

#include <CyGameCaptureCore/SenderNaming.hpp>
#include <CyGameCaptureCore/Version.hpp>

#include <Windows.h>
#include <filesystem>
#include <string>

extern "C" __declspec(dllexport) const char *NAME = "CyGameCaptureRS";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
	"GPU buffer inspector: browse the render targets of the frame, pick the clean game image (before the HUD) "
	"and stream it to OBS through Spout2 without any CPU readback.";

namespace
{
	std::string ProcessGameName()
	{
		wchar_t path[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, path, MAX_PATH);
		const std::filesystem::path exe(path);
		const std::string stem = exe.stem().u8string();
		return cygc::SanitizeGameName(stem);
	}
}

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
	if (!reshade::register_addon(addon_module, reshade_module))
		return false;

	cygc::log::SetVerbose(cygc::config::GetBool("Verbose", false));
	cygc::SetLogoModule(addon_module);
	cygc::SetAddonModule(addon_module);

	// Sender name: "CyGameCaptureRS::<Game>" unless overridden in ReShade.ini ([CYGAMECAPTURE] SenderName=...)
	const std::string override_name = cygc::config::GetString("SenderName", "");
	cygc::SetSenderBaseName(override_name.empty() ? cygc::MakeSenderName(ProcessGameName()) : override_name);

	cygc::RegisterDeviceEvents();
	cygc::RegisterTrackerEvents();
	cygc::RegisterOverlays();

	cygc::log::Info("Add-on initialized (CyGameCaptureRS %s, ReShade API %u, sender '%s'%s)",
		cygc::kVersionString, static_cast<unsigned int>(RESHADE_API_VERSION), cygc::SenderBaseName().c_str(),
		cygc::log::IsVerbose() ? ", verbose tracking on" : "");
	return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
	cygc::UnregisterOverlays();
	cygc::UnregisterTrackerEvents();
	cygc::UnregisterDeviceEvents();

	// Every device context (and therefore every capture) is gone by now, so the bridge holds no GPU work
	cygc::D3D11On12Bridge::Get().Shutdown();

	cygc::log::Info("Add-on unloaded");
	reshade::unregister_addon(addon_module, reshade_module);
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
	return TRUE;
}
