// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

/* CyGameCapture — resource identifiers shared by every binary of the project.
 *
 * Plain C preprocessor only: this is included by the resource compiler as well as by C++ code. */
#pragma once

/* The application icon, first icon resource so the shell picks it for the file itself. */
#define IDI_CYGAMECAPTURE        101

/* The logo as a PNG, for the binaries that draw it in their own interface (the ReShade overlay). */
#define IDR_CYGAMECAPTURE_LOGO   201
