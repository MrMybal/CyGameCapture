// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Localization.hpp"
#include "Addon/Config.hpp"

#include <atomic>
#include <string_view>
#include <unordered_map>

namespace cygc::i18n
{
	namespace
	{
		struct Entry
		{
			const char *english;
			const char *translation;
		};

		const Entry kFrench[] = {
#include "Translations_fr.inl"
		};

		// -1 until read from ReShade.ini, then the Language value
		std::atomic<int> g_language { -1 };

		const std::unordered_map<std::string_view, const char *> &French()
		{
			static const std::unordered_map<std::string_view, const char *> table = [] {
				std::unordered_map<std::string_view, const char *> map;
				map.reserve(std::size(kFrench));
				for (const Entry &entry : kFrench)
					map.emplace(entry.english, entry.translation);
				return map;
			}();
			return table;
		}
	}

	Language Current()
	{
		int value = g_language.load(std::memory_order_relaxed);
		if (value < 0)
		{
			value = config::GetString("Language", "en") == "fr" ? static_cast<int>(Language::French) : static_cast<int>(Language::English);
			g_language.store(value, std::memory_order_relaxed);
		}
		return static_cast<Language>(value);
	}

	void SetCurrent(Language language)
	{
		g_language.store(static_cast<int>(language), std::memory_order_relaxed);
		config::SetString("Language", language == Language::French ? "fr" : "en");
	}

	const char *Code()
	{
		return Current() == Language::French ? "fr" : "en";
	}

	const char *Tr(const char *english)
	{
		if (english == nullptr || Current() == Language::English)
			return english;
		const auto &table = French();
		const auto it = table.find(english);
		return it != table.end() ? it->second : english;
	}
}
