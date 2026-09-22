// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Log.hpp"

#include <reshade.hpp>
#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace
{
	std::atomic<bool> g_verbose{ false };

	void Write(reshade::log::level level, const char *format, va_list args)
	{
		char buffer[2048];
		vsnprintf(buffer, sizeof(buffer), format, args);
		buffer[sizeof(buffer) - 1] = '\0';
		reshade::log::message(level, buffer);
	}
}

namespace cygc::log
{
	void Info(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Write(reshade::log::level::info, format, args);
		va_end(args);
	}
	void Warning(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Write(reshade::log::level::warning, format, args);
		va_end(args);
	}
	void Error(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		Write(reshade::log::level::error, format, args);
		va_end(args);
	}
	void Verbose(const char *format, ...)
	{
		if (!g_verbose.load(std::memory_order_relaxed))
			return;
		va_list args;
		va_start(args, format);
		Write(reshade::log::level::debug, format, args);
		va_end(args);
	}

	bool IsVerbose()
	{
		return g_verbose.load(std::memory_order_relaxed);
	}
	void SetVerbose(bool enabled)
	{
		g_verbose.store(enabled, std::memory_order_relaxed);
	}
}
