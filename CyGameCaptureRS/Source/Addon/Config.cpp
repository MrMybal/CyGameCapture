// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Config.hpp"

#include <reshade.hpp>
#include <cstdlib>
#include <cstdio>

namespace cygc::config
{
	bool GetBool(const char *key, bool default_value)
	{
		bool value = default_value;
		if (!reshade::get_config_value(nullptr, kSection, key, value))
			return default_value;
		return value;
	}
	int GetInt(const char *key, int default_value)
	{
		int value = default_value;
		if (!reshade::get_config_value(nullptr, kSection, key, value))
			return default_value;
		return value;
	}
	float GetFloat(const char *key, float default_value)
	{
		char buffer[64] = {};
		size_t size = sizeof(buffer) - 1;
		if (!reshade::get_config_value(nullptr, kSection, key, buffer, &size) || size == 0)
			return default_value;
		return static_cast<float>(std::atof(buffer));
	}
	std::string GetString(const char *key, const std::string &default_value)
	{
		size_t size = 0;
		if (!reshade::get_config_value(nullptr, kSection, key, nullptr, &size) || size == 0)
			return default_value;
		std::string value(size, '\0');
		reshade::get_config_value(nullptr, kSection, key, value.data(), &size);
		if (size > 0)
			value.resize(size - 1); // strip null terminator
		return value;
	}

	void SetBool(const char *key, bool value)
	{
		reshade::set_config_value(nullptr, kSection, key, value);
	}
	void SetInt(const char *key, int value)
	{
		reshade::set_config_value(nullptr, kSection, key, value);
	}
	void SetFloat(const char *key, float value)
	{
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%g", value);
		reshade::set_config_value(nullptr, kSection, key, static_cast<const char *>(buffer));
	}
	void SetString(const char *key, const std::string &value)
	{
		reshade::set_config_value(nullptr, kSection, key, value.c_str());
	}
}
