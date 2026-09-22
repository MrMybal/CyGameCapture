// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — persistent settings, stored in ReShade.ini under [CYGAMECAPTURE].
// Only small global settings live here (verbose flag, UI filter defaults, ...).
// Per-game capture profiles are a separate feature (Profiles/), not this file.
#pragma once

#include <string>

namespace cygc::config
{
	constexpr const char *kSection = "CYGAMECAPTURE";

	bool GetBool(const char *key, bool default_value);
	int GetInt(const char *key, int default_value);
	float GetFloat(const char *key, float default_value);
	std::string GetString(const char *key, const std::string &default_value);

	void SetBool(const char *key, bool value);
	void SetInt(const char *key, int value);
	void SetFloat(const char *key, float value);
	void SetString(const char *key, const std::string &value);
}
