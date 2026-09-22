// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureOBS — OBS Studio plugin entry points.
//
// OBS loads every *.dll found in its obs-plugins/64bit folder and calls obs_module_load(). The plugin
// registers one input source, "CyGameCapture", which receives the Spout2 senders published by
// CyGameCaptureRS and CyGameCaptureUE.
#include <obs-module.h>

#include <CyGameCaptureCore/Version.hpp>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Cyberalien")
OBS_MODULE_USE_DEFAULT_LOCALE("cygamecapture", "en-US")

void cygc_register_capture_source();

MODULE_EXPORT const char *obs_module_name(void)
{
	return "CyGameCapture";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Receives the GPU buffers published over Spout2 by CyGameCaptureRS (ReShade) and CyGameCaptureUE (Unreal), "
	       "without any CPU readback.";
}

bool obs_module_load(void)
{
	cygc_register_capture_source();
	blog(LOG_INFO, "[CyGameCapture] plugin %s loaded (libobs API %u.%u.%u)", cygc::kVersionString,
		LIBOBS_API_MAJOR_VER, LIBOBS_API_MINOR_VER, LIBOBS_API_PATCH_VER);
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[CyGameCapture] plugin unloaded");
}
