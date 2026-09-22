// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — logging through ReShade's log file.
//
// ReShade automatically prefixes every message with the add-on name, so lines appear as
//   [CyGameCaptureRS] Add-on initialized
// Verbose (per-resource) tracking output only goes out when verbose mode is enabled,
// it is disabled by default because a game can create hundreds of resources per second.
#pragma once

namespace cygc::log
{
	void Info(const char *format, ...);
	void Warning(const char *format, ...);
	void Error(const char *format, ...);

	// Only written when verbose mode is enabled (see SetVerbose / [CYGAMECAPTURE] Verbose=1 in ReShade.ini)
	void Verbose(const char *format, ...);

	bool IsVerbose();
	void SetVerbose(bool enabled);
}
