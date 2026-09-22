// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — the interface's language.
//
// English is the interface's own language: every text is written in English where it is used, wrapped in
// TR(). Another language is a table from those English texts to their translations (Translations_fr.inl
// for French), so a text nobody translated yet simply shows in English instead of breaking anything.
//
// The choice is stored in ReShade.ini, [CYGAMECAPTURE] Language=en|fr, and changes from the overlay
// immediately. Format strings are translated as a whole and keep their conversions in the same order.
#pragma once

namespace cygc::i18n
{
	enum class Language { English, French };

	Language Current();
	void SetCurrent(Language language);
	/** "en" or "fr": what the AI assistant replies in unless AILanguage says otherwise. */
	const char *Code();

	/** The text in the current language; `english` itself when there is no translation. */
	const char *Tr(const char *english);
}

#define TR(text) ::cygc::i18n::Tr(text)
