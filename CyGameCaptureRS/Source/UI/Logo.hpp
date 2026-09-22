// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — the CyGameCapture logo, as a texture the overlay can draw.
//
// The PNG is embedded in the add-on's own resources (IDR_CYGAMECAPTURE_LOGO), so there is no file to
// ship next to the .addon64 and nothing to lose. It is decoded once per process with WIC, which is part
// of Windows, rather than with a vendored decoder; then each device gets an ordinary shader-readable
// texture made from those pixels, drawn with ImGui::Image like the buffer preview.
//
// This is static interface artwork uploaded once, not a captured frame: the "GPU only" rule of the
// capture path is about game images, and nothing here ever reads a GPU resource back.
#pragma once

#include <reshade.hpp>

#include <Windows.h>
#include <cstdint>
#include <functional>

namespace cygc
{
	/** The module the logo resource lives in; set by AddonInit before anything is drawn. */
	void SetLogoModule(HMODULE addon_module);

	class LogoTexture
	{
	public:
		explicit LogoTexture(reshade::api::device *device);
		~LogoTexture();

		LogoTexture(const LogoTexture &) = delete;
		LogoTexture &operator=(const LogoTexture &) = delete;

		/**
		 * The logo's shader resource view, created on first use. 0 when it could not be made (resource
		 * missing, decoder unavailable, texture creation refused); the overlay then simply shows no logo.
		 */
		reshade::api::resource_view View();

		/** Told about the texture once it exists, so the owner can keep it out of the resource tracker. */
		void SetResourceNotice(std::function<void(reshade::api::resource)> on_created) { on_created_ = std::move(on_created); }

	private:
		reshade::api::device *const device_;
		reshade::api::resource texture_ = {};
		reshade::api::resource_view view_ = {};
		bool attempted_ = false;
		std::function<void(reshade::api::resource)> on_created_;
	};
}
