// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <Windows.h>
#include <shellapi.h>
#include <filesystem>
#include <iterator>
#include <string>

namespace cygc
{
	inline std::wstring GuideWidePath(const char *utf8)
	{
		if (utf8 == nullptr || *utf8 == '\0') return {};
		const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
		if (size <= 0) return {};
		std::wstring result(static_cast<size_t>(size), L'\0');
		if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, result.data(), size)) return {};
		result.pop_back();
		return result;
	}

	// Only a known local HTML file; never fall back silently to a network URL.
	inline bool OpenUserGuideFile(const std::filesystem::path &file)
	{
		const DWORD attributes = GetFileAttributesW(file.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return false;
		return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}

	inline bool OpenUserGuideBesideModule(bool french)
	{
		// Resolve the add-on folder, including custom AddonPath, rather than the game's CWD.
		static const wchar_t module_anchor = 0;
		HMODULE module = nullptr;
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &module_anchor, &module)) return false;
		wchar_t path[32768] = {};
		const DWORD length = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
		if (length == 0 || length >= std::size(path)) return false;
		return OpenUserGuideFile(std::filesystem::path(path).parent_path() / L"Documentation" / L"UserGuide" / (french ? L"fr.html" : L"en.html"));
	}
}
