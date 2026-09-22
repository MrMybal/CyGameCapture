// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureSpoutReceiverTest — headless Spout receiver used to validate CyGameCaptureRS
// without OBS. It behaves like the OBS Spout2 plugin (SpoutDX receiver, opens the sender's
// shared texture) and dumps received frames to .bmp files.
//
//   CyGameCaptureSpoutReceiverTest [--list] [--sender <name>] [--frames N] [--out <dir>] [--timeout-ms T] [--alpha]
//
// Exit code 0 when at least one frame was received, 1 otherwise.
#include <SpoutDX.h>
#include <CyGameCaptureCore/SenderNaming.hpp>
#include <CyGameCaptureCore/FrameSync.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
	/**
	* Reads an R16G16B16A16_UNORM sender, which is what the linearised depth stream publishes.
	*
	* SpoutDX' own ReadTexurePixels assumes four *bytes* per pixel, so it would silently truncate a
	* 16-bit texture. This does the staging copy itself, reports the real 16-bit range so the precision
	* can be checked, and fills an 8-bit RGBA buffer (high byte of each channel) for the .bmp dump.
	*/
	bool ReadTexture16(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *source,
		std::vector<uint8_t> &out_rgba8, uint16_t &out_min, uint16_t &out_max, double &out_mean)
	{
		if (device == nullptr || context == nullptr || source == nullptr)
			return false;

		D3D11_TEXTURE2D_DESC desc = {};
		source->GetDesc(&desc);
		const bool half_float = desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT;
		if (desc.Format != DXGI_FORMAT_R16G16B16A16_UNORM && !half_float)
			return false;

		D3D11_TEXTURE2D_DESC staging = desc;
		staging.Usage = D3D11_USAGE_STAGING;
		staging.BindFlags = 0;
		staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		staging.MiscFlags = 0;

		ID3D11Texture2D *readback = nullptr;
		if (FAILED(device->CreateTexture2D(&staging, nullptr, &readback)) || readback == nullptr)
			return false;

		context->CopyResource(readback, source);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(context->Map(readback, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			readback->Release();
			return false;
		}

		out_rgba8.assign(static_cast<size_t>(desc.Width) * desc.Height * 4, 0);
		out_min = 0xFFFF;
		out_max = 0;
		uint64_t sum = 0;

		// A half float has to be decoded before it means anything; an HDR buffer is also usually above 1,
		// so the .bmp gets a plain Reinhard curve rather than a clamp that would flatten every highlight.
		auto half_to_float = [](uint16_t bits) {
			const uint32_t sign = static_cast<uint32_t>(bits & 0x8000u) << 16;
			uint32_t exponent = (bits >> 10) & 0x1Fu;
			uint32_t mantissa = bits & 0x3FFu;
			if (exponent == 0)
			{
				if (mantissa == 0)
					return 0.0f;
				while ((mantissa & 0x400u) == 0) { mantissa <<= 1; --exponent; }
				++exponent;
				mantissa &= 0x3FFu;
			}
			else if (exponent == 31)
			{
				exponent = 255;
			}
			const uint32_t bits32 = sign | ((exponent + 112) << 23) | (mantissa << 13);
			float result;
			memcpy(&result, &bits32, sizeof(result));
			return result;
		};
		auto to_byte = [&](uint16_t raw) -> uint8_t {
			if (!half_float)
				return static_cast<uint8_t>(raw >> 8);
			const float value = half_to_float(raw);
			const float mapped = value <= 0.0f ? 0.0f : value / (1.0f + value);   // Reinhard
			const float encoded = powf(mapped, 1.0f / 2.2f) * 255.0f;
			return static_cast<uint8_t>(encoded < 0.0f ? 0.0f : (encoded > 255.0f ? 255.0f : encoded));
		};

		for (unsigned int y = 0; y < desc.Height; ++y)
		{
			const auto *const row = reinterpret_cast<const uint16_t *>(static_cast<const uint8_t *>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch);
			for (unsigned int x = 0; x < desc.Width; ++x)
			{
				const uint16_t red = row[x * 4 + 0];
				out_min = red < out_min ? red : out_min;
				out_max = red > out_max ? red : out_max;
				sum += red;

				const size_t destination = (static_cast<size_t>(y) * desc.Width + x) * 4;
				out_rgba8[destination + 0] = to_byte(row[x * 4 + 0]);
				out_rgba8[destination + 1] = to_byte(row[x * 4 + 1]);
				out_rgba8[destination + 2] = to_byte(row[x * 4 + 2]);
				out_rgba8[destination + 3] = 0xFF;
			}
		}

		context->Unmap(readback, 0);
		readback->Release();
		out_mean = static_cast<double>(sum) / (static_cast<double>(desc.Width) * desc.Height);
		return true;
	}

	/**
	* Reads an R10G10B10A2_UNORM sender, which is what a game using an HDR10 style back buffer publishes.
	*
	* The size matches what ReadTexurePixels expects, four bytes per pixel, so it does not fail: it just
	* hands back the packed words as if they were RGBA8 and the colours come out wrong. Unpacking the
	* three ten bit channels is the only way to see what the buffer really holds.
	*/
	bool ReadTexture1010102(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *source,
		std::vector<uint8_t> &out_rgba8)
	{
		if (device == nullptr || context == nullptr || source == nullptr)
			return false;

		D3D11_TEXTURE2D_DESC desc = {};
		source->GetDesc(&desc);
		if (desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM)
			return false;

		D3D11_TEXTURE2D_DESC staging = desc;
		staging.Usage = D3D11_USAGE_STAGING;
		staging.BindFlags = 0;
		staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		staging.MiscFlags = 0;

		ID3D11Texture2D *readback = nullptr;
		if (FAILED(device->CreateTexture2D(&staging, nullptr, &readback)) || readback == nullptr)
			return false;
		context->CopyResource(readback, source);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(context->Map(readback, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			readback->Release();
			return false;
		}

		out_rgba8.assign(static_cast<size_t>(desc.Width) * desc.Height * 4, 0);
		for (unsigned int y = 0; y < desc.Height; ++y)
		{
			const auto *const row = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch);
			for (unsigned int x = 0; x < desc.Width; ++x)
			{
				const uint32_t packed = row[x];
				const size_t destination = (static_cast<size_t>(y) * desc.Width + x) * 4;
				out_rgba8[destination + 0] = static_cast<uint8_t>(((packed >> 0) & 0x3FFu) >> 2);
				out_rgba8[destination + 1] = static_cast<uint8_t>(((packed >> 10) & 0x3FFu) >> 2);
				out_rgba8[destination + 2] = static_cast<uint8_t>(((packed >> 20) & 0x3FFu) >> 2);
				out_rgba8[destination + 3] = 0xFF;
			}
		}

		context->Unmap(readback, 0);
		readback->Release();
		return true;
	}

	bool WriteBmp(const std::string &path, const uint8_t *rgba, unsigned int width, unsigned int height)
	{
		FILE *file = fopen(path.c_str(), "wb");
		if (file == nullptr)
			return false;

		const uint32_t row_size = width * 3;
		const uint32_t padded_row = (row_size + 3) & ~3u;
		const uint32_t image_size = padded_row * height;

#pragma pack(push, 1)
		struct { uint16_t type; uint32_t size; uint16_t r1, r2; uint32_t offset; } file_header = { 0x4D42, 54 + image_size, 0, 0, 54 };
		struct { uint32_t size; int32_t w, h; uint16_t planes, bpp; uint32_t compression, image_size; int32_t xppm, yppm; uint32_t colors, important; } info_header =
			{ 40, static_cast<int32_t>(width), static_cast<int32_t>(height), 1, 24, 0, image_size, 2835, 2835, 0, 0 };
#pragma pack(pop)
		fwrite(&file_header, sizeof(file_header), 1, file);
		fwrite(&info_header, sizeof(info_header), 1, file);

		std::vector<uint8_t> row(padded_row, 0);
		for (unsigned int y = 0; y < height; ++y)
		{
			const uint8_t *src = rgba + static_cast<size_t>(height - 1 - y) * width * 4;   // BMP is bottom-up
			for (unsigned int x = 0; x < width; ++x)
			{
				row[x * 3 + 0] = src[x * 4 + 2];
				row[x * 3 + 1] = src[x * 4 + 1];
				row[x * 3 + 2] = src[x * 4 + 0];
			}
			fwrite(row.data(), 1, padded_row, file);
		}
		fclose(file);
		return true;
	}

	uint64_t Checksum(const std::vector<uint8_t> &data)
	{
		uint64_t hash = 1469598103934665603ull;
		for (const uint8_t byte : data)
			hash = (hash ^ byte) * 1099511628211ull;
		return hash;
	}
}

int main(int argc, char **argv)
{
	std::string sender_name;
	std::string out_dir = ".";
	int frames = 3;
	int timeout_ms = 15000;
	bool list_only = false;
	bool show_alpha = false;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--list") == 0) list_only = true;
		else if (strcmp(argv[i], "--sender") == 0 && i + 1 < argc) sender_name = argv[++i];
		else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = atoi(argv[++i]);
		else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_dir = argv[++i];
		else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) timeout_ms = atoi(argv[++i]);
		// The HUD stream carries its mask in alpha, which a .bmp cannot show: this writes the alpha
		// channel into R, G and B instead, so the isolated interface is visible as a grey-scale mask.
		else if (strcmp(argv[i], "--alpha") == 0) show_alpha = true;
	}

	// Spout's own diagnostics on the console: shows exactly why a shared handle cannot be opened
	EnableSpoutLog();
	SetSpoutLogLevel(SPOUT_LOG_VERBOSE);

	spoutDX receiver;
	if (!receiver.OpenDirectX11())
	{
		printf("ERROR: could not create a D3D11 device\n");
		return 1;
	}

	// Enumerate senders (this is exactly what CyGameCaptureOBS will do to auto-detect games)
	const int count = receiver.GetSenderCount();
	printf("Spout senders: %d\n", count);
	std::string first_cygc_sender;
	for (int i = 0; i < count; ++i)
	{
		char name[256] = {};
		if (!receiver.GetSender(i, name, sizeof(name)))
			continue;
		unsigned int w = 0, h = 0;
		HANDLE handle = nullptr;
		DWORD format = 0;
		receiver.GetSenderInfo(name, w, h, handle, format);
		const bool is_cygc = cygc::IsCyGameCaptureSender(name);
		printf("  %s%s  %ux%u format=%lu handle=0x%p", is_cygc ? "[CyGameCapture] " : "", name, w, h, format, handle);

		// Cross-process frame index, published by the producer next to the sender (FrameSync.hpp)
		cygc::FrameSyncRecord sync;
		if (cygc::ReadFrameSync(name, sync))
			printf("  [sync frame %llu group %llu pid %u]", static_cast<unsigned long long>(sync.frame_index),
				static_cast<unsigned long long>(sync.group_id), sync.owner_pid);
		printf("\n");
		if (is_cygc && first_cygc_sender.empty())
			first_cygc_sender = name;
	}
	if (list_only)
		return 0;

	if (sender_name.empty())
		sender_name = first_cygc_sender;
	if (sender_name.empty())
	{
		printf("ERROR: no CyGameCaptureRS sender found (use --sender <name> to pick another one)\n");
		return 1;
	}
	printf("Receiving from '%s' ...\n", sender_name.c_str());
	receiver.SetReceiverName(sender_name.c_str());

	const auto start = std::chrono::steady_clock::now();
	int received = 0;
	uint64_t last_checksum = 0;
	int changed = 0;
	std::vector<uint8_t> pixels;

	while (received < frames)
	{
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > timeout_ms)
		{
			printf("Timeout after %d ms (%d frame(s) received)\n", timeout_ms, received);
			break;
		}

		if (!receiver.ReceiveTexture())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			continue;
		}
		// ReceiveTexture() consumes the "updated" flag internally, so size changes are detected here
		const unsigned int width = receiver.GetSenderWidth();
		const unsigned int height = receiver.GetSenderHeight();
		if (width == 0 || height == 0)
			continue;
		if (pixels.size() != static_cast<size_t>(width) * height * 4)
		{
			printf("Sender connected: %ux%u format=%lu fps=%.1f\n", width, height, static_cast<unsigned long>(receiver.GetSenderFormat()), receiver.GetSenderFps());
			pixels.assign(static_cast<size_t>(width) * height * 4, 0);
		}

		// Test tool only: CPU readback is fine here (never in the add-on itself)
		const unsigned long format_now = static_cast<unsigned long>(receiver.GetSenderFormat());
		bool is_16bit = false;
		uint16_t depth_min = 0, depth_max = 0;
		double depth_mean = 0.0;
		if (format_now == 11 || format_now == 10)   // R16G16B16A16_UNORM (depth) and _FLOAT (HDR scene colour)
		{
			if (!ReadTexture16(receiver.GetDX11Device(), receiver.GetDX11Context(), receiver.GetSenderTexture(),
					pixels, depth_min, depth_max, depth_mean))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
				continue;
			}
			is_16bit = true;
		}
		else if (format_now == 24 &&   // DXGI_FORMAT_R10G10B10A2_UNORM
			ReadTexture1010102(receiver.GetDX11Device(), receiver.GetDX11Context(), receiver.GetSenderTexture(), pixels))
		{
			// unpacked above
		}
		else if (!receiver.ReadTexurePixels(receiver.GetSenderTexture(), pixels.data()))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			continue;
		}

		const uint64_t checksum = Checksum(pixels);
		if (checksum != last_checksum)
		{
			if (received > 0)
				changed++;
			last_checksum = checksum;
		}

		// BGRA senders (DXGI 87 / 88 / 91) come back with B and R swapped relative to the RGBA assumption of WriteBmp
		const unsigned long sender_format = format_now;
		if (sender_format == 87 || sender_format == 88 || sender_format == 91)
		{
			for (size_t i = 0; i + 3 < pixels.size(); i += 4)
				std::swap(pixels[i], pixels[i + 2]);
		}
		uint64_t sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
		for (size_t i = 0; i < pixels.size(); i += 4)
		{
			sum_r += pixels[i + 0];
			sum_g += pixels[i + 1];
			sum_b += pixels[i + 2];
			sum_a += pixels[i + 3];
		}
		if (show_alpha)
		{
			for (size_t i = 0; i < pixels.size(); i += 4)
				pixels[i + 0] = pixels[i + 1] = pixels[i + 2] = pixels[i + 3];
		}
		const double n = static_cast<double>(pixels.size() / 4);
		const std::string path = out_dir + "\\cygc_frame_" + std::to_string(received) + ".bmp";
		WriteBmp(path, pixels.data(), receiver.GetSenderWidth(), receiver.GetSenderHeight());
		if (is_16bit)
			printf("Frame %d: checksum=%016llx 16-bit R min=%u max=%u mean=%.1f (%.4f..%.4f) -> %s\n", received,
				static_cast<unsigned long long>(checksum), depth_min, depth_max, depth_mean,
				depth_min / 65535.0, depth_max / 65535.0, path.c_str());
		else
			printf("Frame %d: checksum=%016llx mean RGB=(%.1f, %.1f, %.1f) alpha=%.1f -> %s\n", received, static_cast<unsigned long long>(checksum), sum_r / n, sum_g / n, sum_b / n, sum_a / n, path.c_str());
		received++;
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}

	receiver.ReleaseReceiver();
	receiver.CloseDirectX11();

	printf("Received %d frame(s), %d content change(s) between frames\n", received, changed);
	return received > 0 ? 0 : 1;
}
