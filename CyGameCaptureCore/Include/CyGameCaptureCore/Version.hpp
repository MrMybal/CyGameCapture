// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureCore — version information shared by all CyGameCapture components.
#pragma once

#define CYGC_VERSION_MAJOR 0
#define CYGC_VERSION_MINOR 1
#define CYGC_VERSION_PATCH 0
#define CYGC_VERSION_STRING "0.1.0"

// Shown in the binaries' version resources and in the About box of each interface
#define CYGC_COPYRIGHT_STRING  "Copyright (C) 2026 Cyberalien"
#define CYGC_LEGAL_COPYRIGHT   "Copyright (C) 2026 Cyberalien. Licensed under the GNU AGPL v3 or later."
#define CYGC_PROJECT_URL       "https://github.com/MrMybal/CyGameCapture"

#ifndef RC_INVOKED
namespace cygc
{
	constexpr unsigned int kVersionMajor = CYGC_VERSION_MAJOR;
	constexpr unsigned int kVersionMinor = CYGC_VERSION_MINOR;
	constexpr unsigned int kVersionPatch = CYGC_VERSION_PATCH;
	constexpr const char *kVersionString = CYGC_VERSION_STRING;
	constexpr const char *kCopyrightString = CYGC_COPYRIGHT_STRING;
	constexpr const char *kProjectUrl = CYGC_PROJECT_URL;
}
#endif
