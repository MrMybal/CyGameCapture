// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Logo.hpp"
#include "Addon/Log.hpp"

#include <CyGameCaptureBranding.h>

#include <wincodec.h>
#include <mutex>
#include <vector>

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		HMODULE g_logo_module = nullptr;

		struct DecodedImage
		{
			uint32_t width = 0;
			uint32_t height = 0;
			std::vector<uint8_t> rgba;   // straight alpha, which is what ImGui blends with
		};

		template <typename T>
		void SafeRelease(T *&object)
		{
			if (object != nullptr)
				object->Release();
			object = nullptr;
		}

		/**
		 * PNG bytes from the add-on resources to RGBA8 pixels.
		 *
		 * WIC needs COM on the calling thread. The overlay runs on whatever thread the game renders from,
		 * which may already be in an apartment of either kind: an existing one is used as it is
		 * (RPC_E_CHANGED_MODE just means it is the other kind, which WIC does not mind), and only an
		 * initialisation made here is undone here.
		 */
		DecodedImage Decode()
		{
			DecodedImage image;
			if (g_logo_module == nullptr)
				return image;

			const HRSRC info = FindResourceW(g_logo_module, MAKEINTRESOURCEW(IDR_CYGAMECAPTURE_LOGO), MAKEINTRESOURCEW(10) /* RT_RCDATA, wide */);
			const HGLOBAL handle = info != nullptr ? LoadResource(g_logo_module, info) : nullptr;
			const void *const bytes = handle != nullptr ? LockResource(handle) : nullptr;
			const DWORD size = info != nullptr ? SizeofResource(g_logo_module, info) : 0;
			if (bytes == nullptr || size == 0)
			{
				log::Warning("Logo resource not found in the add-on");
				return image;
			}

			const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			const bool uninitialize = SUCCEEDED(com);

			IWICImagingFactory *factory = nullptr;
			IWICStream *stream = nullptr;
			IWICBitmapDecoder *decoder = nullptr;
			IWICBitmapFrameDecode *frame = nullptr;
			IWICFormatConverter *converter = nullptr;

			HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
			if (SUCCEEDED(hr))
				hr = factory->CreateStream(&stream);
			if (SUCCEEDED(hr))
				hr = stream->InitializeFromMemory(static_cast<BYTE *>(const_cast<void *>(bytes)), size);
			if (SUCCEEDED(hr))
				hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
			if (SUCCEEDED(hr))
				hr = decoder->GetFrame(0, &frame);
			if (SUCCEEDED(hr))
				hr = factory->CreateFormatConverter(&converter);
			if (SUCCEEDED(hr))
				hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

			UINT width = 0, height = 0;
			if (SUCCEEDED(hr))
				hr = converter->GetSize(&width, &height);
			if (SUCCEEDED(hr) && width > 0 && height > 0)
			{
				image.rgba.resize(static_cast<size_t>(width) * height * 4);
				hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(image.rgba.size()), image.rgba.data());
				if (SUCCEEDED(hr))
				{
					image.width = width;
					image.height = height;
				}
				else
					image.rgba.clear();
			}
			if (FAILED(hr))
				log::Warning("Could not decode the logo (HRESULT 0x%08X)", static_cast<unsigned int>(hr));

			SafeRelease(converter);
			SafeRelease(frame);
			SafeRelease(decoder);
			SafeRelease(stream);
			SafeRelease(factory);
			if (uninitialize)
				CoUninitialize();
			return image;
		}

		/** Decoded once for the whole process, whatever the number of devices. */
		const DecodedImage &Logo()
		{
			static std::once_flag once;
			static DecodedImage image;
			std::call_once(once, [] { image = Decode(); });
			return image;
		}
	}

	void SetLogoModule(HMODULE addon_module)
	{
		g_logo_module = addon_module;
	}

	LogoTexture::LogoTexture(device *device) :
		device_(device)
	{
	}

	LogoTexture::~LogoTexture()
	{
		if (view_ != 0)
			device_->destroy_resource_view(view_);
		if (texture_ != 0)
			device_->destroy_resource(texture_);
	}

	resource_view LogoTexture::View()
	{
		if (view_ != 0 || attempted_)
			return view_;
		attempted_ = true;

		const DecodedImage &image = Logo();
		if (image.width == 0)
			return view_;

		const resource_desc desc(image.width, image.height, 1, 1, format::r8g8b8a8_unorm, 1,
			memory_heap::default_, resource_usage::shader_resource);
		// ReShade takes the initial data through a non-const pointer but only reads it
		const subresource_data initial { const_cast<uint8_t *>(image.rgba.data()), image.width * 4, image.width * image.height * 4 };

		if (!device_->create_resource(desc, &initial, resource_usage::shader_resource, &texture_))
		{
			texture_ = {};
			log::Warning("Could not create the logo texture");
			return view_;
		}
		if (on_created_)
			on_created_(texture_);
		if (!device_->create_resource_view(texture_, resource_usage::shader_resource, resource_view_desc(format::r8g8b8a8_unorm), &view_))
		{
			view_ = {};
			device_->destroy_resource(texture_);
			texture_ = {};
		}
		return view_;
	}
}
