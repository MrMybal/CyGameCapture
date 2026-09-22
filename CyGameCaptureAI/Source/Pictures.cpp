// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureAI — the pictures the add-on rendered, turned into PNG files for the AI client.
//
// The add-on renders every picture into one shared texture (an atlas of tiles) on the game's GPU and
// hands over its share handle. Reading it back happens here, in this separate process: the game itself
// never copies a frame to the CPU. Opening the handle on a device of our own is exactly what any Spout
// receiver does with a sender's texture.
#include "Pictures.hpp"
#include "AiClients.hpp"

#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace cygc
{
	namespace
	{
		/** A tile as an opaque 24-bit PNG: the game's alpha channel means nothing for a picture to look at. */
		bool WritePng(IWICImagingFactory *factory, const std::wstring &path, const uint8_t *rgba, uint32_t row_pitch,
			uint32_t width, uint32_t height, std::string &error)
		{
			std::vector<uint8_t> bgr(static_cast<size_t>(width) * height * 3);
			for (uint32_t y = 0; y < height; ++y)
			{
				const uint8_t *src = rgba + static_cast<size_t>(y) * row_pitch;
				uint8_t *dst = bgr.data() + static_cast<size_t>(y) * width * 3;
				for (uint32_t x = 0; x < width; ++x, src += 4, dst += 3)
				{
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
				}
			}

			ComPtr<IWICStream> stream;
			ComPtr<IWICBitmapEncoder> encoder;
			ComPtr<IWICBitmapFrameEncode> frame;
			HRESULT hr = factory->CreateStream(&stream);
			if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
			if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
			if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
			if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, nullptr);
			if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
			if (SUCCEEDED(hr)) hr = frame->SetSize(width, height);
			WICPixelFormatGUID pixel_format = GUID_WICPixelFormat24bppBGR;
			if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&pixel_format);
			if (SUCCEEDED(hr) && pixel_format != GUID_WICPixelFormat24bppBGR)
				hr = E_FAIL;
			if (SUCCEEDED(hr)) hr = frame->WritePixels(height, width * 3, static_cast<UINT>(bgr.size()), bgr.data());
			if (SUCCEEDED(hr)) hr = frame->Commit();
			if (SUCCEEDED(hr)) hr = encoder->Commit();
			if (FAILED(hr))
			{
				char text[64];
				snprintf(text, sizeof(text), "PNG encoding failed (0x%08X)", static_cast<unsigned int>(hr));
				error = text;
				return false;
			}
			return true;
		}
	}

	bool ExportTiles(uint64_t share_handle, const AtlasLayout &layout, const std::vector<TileRequest> &tiles,
		const std::wstring &folder, std::vector<std::wstring> &out_paths, std::string &error)
	{
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context)))
		{
			error = "No Direct3D 11 device";
			return false;
		}

		ComPtr<ID3D11Texture2D> shared;
		if (FAILED(device->OpenSharedResource(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(share_handle)), IID_PPV_ARGS(&shared))))
		{
			error = Msg("Could not open the pictures shared by the game (was it closed?)", "Impossible d'ouvrir les images partagées par le jeu (a-t-il été fermé ?)");
			return false;
		}
		D3D11_TEXTURE2D_DESC desc = {};
		shared->GetDesc(&desc);
		if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM || desc.Width < layout.width || desc.Height < layout.height)
		{
			error = "The shared pictures do not have the expected layout";
			return false;
		}

		D3D11_TEXTURE2D_DESC staging_desc = desc;
		staging_desc.Usage = D3D11_USAGE_STAGING;
		staging_desc.BindFlags = 0;
		staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		staging_desc.MiscFlags = 0;
		ComPtr<ID3D11Texture2D> staging;
		if (FAILED(device->CreateTexture2D(&staging_desc, nullptr, &staging)))
		{
			error = "Could not create the readback texture";
			return false;
		}
		context->CopyResource(staging.Get(), shared.Get());
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			error = "Could not read the shared pictures";
			return false;
		}

		ComPtr<IWICImagingFactory> factory;
		const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
		if (!ok)
			error = "Windows Imaging Component unavailable";

		for (const TileRequest &tile : tiles)
		{
			if (!ok)
				break;
			const uint32_t column = tile.index % layout.columns;
			const uint32_t row = tile.index / layout.columns;
			const uint32_t x = column * layout.tile_width;
			const uint32_t y = row * layout.tile_height;
			if (x + layout.tile_width > desc.Width || y + layout.tile_height > desc.Height)
				continue;
			const uint8_t *const origin = static_cast<const uint8_t *>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch + static_cast<size_t>(x) * 4;
			wchar_t name[32];
			swprintf(name, 32, L"picture_%02u.png", tile.index);
			const std::wstring path = folder + L"\\" + name;
			ok = WritePng(factory.Get(), path, origin, mapped.RowPitch, layout.tile_width, layout.tile_height, error);
			if (ok)
				out_paths.push_back(path);
		}

		context->Unmap(staging.Get(), 0);
		factory.Reset();
		if (SUCCEEDED(com))
			CoUninitialize();
		return ok && !out_paths.empty();
	}
}
