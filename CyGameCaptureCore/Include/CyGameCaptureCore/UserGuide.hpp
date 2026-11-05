// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <Windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include "cydoc_link.hpp"
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

	/**
	 * @doc native-help-api
	 * @locale en
	 * @title Native Windows help API
	 * @section API
	 * @symbol cygc::OpenUserGuideFile
	 * Open a shipped CyDoc page from the native ReShade or OBS component.
	 *
	 * ## Local file URL {#local-file-url}
	 * `OpenUserGuideFile(file, french, page, anchor, media)` checks the local index,
	 * converts its path with Windows `UrlCreateFromPathW`, then uses CyDoc's C++17
	 * route helper. Unicode, spaces, percent signs and hash signs in paths are
	 * handled before adding the CyDoc fragment. No HTTP fallback is used.
	 *
	 * ## Parameters and result {#parameters}
	 * `file` is the installed `index.html`; `french` selects fr or en; `page` is a
	 * stable page ID. Optional `anchor` and `media` target a section or image.
	 * The return value indicates whether Windows accepted the local open request,
	 * not whether the browser finished rendering. Missing files and shell errors
	 * return false so the host can display its translated error message.
	 * `OpenUserGuideBesideModule` resolves the add-on directory, not the game CWD.
	 */
	inline std::wstring UserGuideUrl(const std::filesystem::path &file, bool french = false,
		const char *page = "overview", const char *anchor = "", const char *media = "")
	{
		std::wstring url(32768, L'\0');
		DWORD length = static_cast<DWORD>(url.size());
		if (FAILED(UrlCreateFromPathW(file.c_str(), url.data(), &length, 0))) return {};
		url.resize(wcslen(url.c_str()));
		// Shlwapi may escape Latin characters using the Windows code page (%E7).
		// Round-trip that canonical path, then encode its Unicode as UTF-8 for browsers.
		std::wstring unescaped(32768, L'\0');
		length = static_cast<DWORD>(unescaped.size());
		if (FAILED(UrlUnescapeW(url.data(), unescaped.data(), &length, 0))) return {};
		unescaped.resize(wcslen(unescaped.c_str()));
		const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, unescaped.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (bytes <= 0) return {};
		std::string base(static_cast<size_t>(bytes), '\0');
		if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, unescaped.c_str(), -1, base.data(), bytes, nullptr, nullptr)) return {};
		base.pop_back();
		std::string encoded;
		constexpr char hex[] = "0123456789ABCDEF";
		for (unsigned char c : base)
		{
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~' || c == ':' || c == '/') encoded += static_cast<char>(c);
			else { encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15]; }
		}
		std::vector<std::pair<std::string, std::string>> state;
		if (*media) state.emplace_back("media", media);
		return GuideWidePath(cydoc_native::link(encoded, french ? "fr" : "en", page, anchor, state).c_str());
	}

	inline bool OpenUserGuideFile(const std::filesystem::path &file, bool french = false,
		const char *page = "overview", const char *anchor = "", const char *media = "")
	{
		const DWORD attributes = GetFileAttributesW(file.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return false;
		const std::wstring url = UserGuideUrl(file, french, page, anchor, media);
		if (url.empty()) return false;
		return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}

	inline bool OpenUserGuideBesideModule(bool french, const char *page = "overview", const char *anchor = "", const char *media = "")
	{
		// Resolve the add-on folder, including custom AddonPath, rather than the game's CWD.
		static const wchar_t module_anchor = 0;
		HMODULE module = nullptr;
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &module_anchor, &module)) return false;
		wchar_t path[32768] = {};
		const DWORD length = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
		if (length == 0 || length >= std::size(path)) return false;
		return OpenUserGuideFile(std::filesystem::path(path).parent_path() / L"Documentation" / L"UserGuide" / L"index.html", french, page, anchor, media);
	}
}
