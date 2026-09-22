// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureCore — Spout sender naming convention.
//
//   CyGameCaptureRS::<GameName>          first instance
//   CyGameCaptureRS::<GameName>::2       second instance of the same game, etc.
//
// <GameName> is derived from the executable name with common engine suffixes
// removed (e.g. "Beyond-Win64-Shipping.exe" -> "Beyond").
// Both the ReShade add-on (sender side) and the future OBS plugin (receiver side)
// rely on this header so the convention is defined exactly once.
#pragma once

#include <string>
#include <string_view>
#include <cctype>

namespace cygc
{
	// Common prefix of every Spout sender produced by CyGameCaptureRS.
	constexpr std::string_view kSenderPrefix = "CyGameCaptureRS::";

	// The Unreal plugin publishes "CyGameCaptureUE::<Project>::<Stream>". CyGameCaptureOBS recognises both.
	constexpr std::string_view kUnrealSenderPrefix = "CyGameCaptureUE::";

	// Spout sender names are limited to 256 bytes including the terminator.
	constexpr size_t kMaxSenderNameLength = 255;

	inline bool EndsWithNoCase(std::string_view text, std::string_view suffix)
	{
		if (text.size() < suffix.size())
			return false;
		for (size_t i = 0; i < suffix.size(); ++i)
		{
			const unsigned char a = static_cast<unsigned char>(text[text.size() - suffix.size() + i]);
			const unsigned char b = static_cast<unsigned char>(suffix[i]);
			if (std::tolower(a) != std::tolower(b))
				return false;
		}
		return true;
	}

	// "Beyond-Win64-Shipping" -> "Beyond", "Cyberpunk2077" -> "Cyberpunk2077"
	inline std::string SanitizeGameName(std::string_view exe_stem)
	{
		std::string name(exe_stem);

		static constexpr std::string_view suffixes[] = {
			"-Win64-Shipping", "-WinGDK-Shipping", "-Win64-Test", "-Win64-DebugGame", "-Win64-Debug", "_Win64_Shipping",
		};
		for (const std::string_view suffix : suffixes)
		{
			if (name.size() > suffix.size() && EndsWithNoCase(name, suffix))
			{
				name.erase(name.size() - suffix.size());
				break;
			}
		}

		// Sender names travel through shared memory as plain ASCII and "::" is our separator
		for (char &c : name)
			if (static_cast<unsigned char>(c) < 0x20 || c == ':' || c == '\\' || c == '/' || c == '"')
				c = '_';

		if (name.empty())
			name = "Game";
		return name;
	}

	inline std::string MakeSenderName(std::string_view game_name, unsigned int instance = 1)
	{
		std::string name(kSenderPrefix);
		name += game_name;
		if (instance > 1)
			name += "::" + std::to_string(instance);
		if (name.size() > kMaxSenderNameLength)
			name.resize(kMaxSenderNameLength);
		return name;
	}

	inline bool IsReShadeSender(std::string_view sender_name)
	{
		return sender_name.size() > kSenderPrefix.size() && sender_name.substr(0, kSenderPrefix.size()) == kSenderPrefix;
	}

	inline bool IsUnrealSender(std::string_view sender_name)
	{
		return sender_name.size() > kUnrealSenderPrefix.size() && sender_name.substr(0, kUnrealSenderPrefix.size()) == kUnrealSenderPrefix;
	}

	// Any sender produced by this project, whatever the component it comes from.
	inline bool IsCyGameCaptureSender(std::string_view sender_name)
	{
		return IsReShadeSender(sender_name) || IsUnrealSender(sender_name);
	}

	// "CyGameCaptureRS::Beyond" -> "Beyond", "CyGameCaptureUE::Demo::Viewport" -> "Demo::Viewport"
	inline std::string_view SenderDisplayName(std::string_view sender_name)
	{
		if (IsReShadeSender(sender_name))
			return sender_name.substr(kSenderPrefix.size());
		if (IsUnrealSender(sender_name))
			return sender_name.substr(kUnrealSenderPrefix.size());
		return sender_name;
	}

	// Which component published a sender, for display in the OBS source properties.
	inline const char *SenderOriginName(std::string_view sender_name)
	{
		if (IsReShadeSender(sender_name))
			return "ReShade";
		if (IsUnrealSender(sender_name))
			return "Unreal";
		return "Spout";
	}
}
