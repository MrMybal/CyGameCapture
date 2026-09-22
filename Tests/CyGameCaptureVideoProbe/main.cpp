// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureVideoProbe - what actually survives in a recorded file.
//
// Usage: CyGameCaptureVideoProbe <file> [frames]
//
// It decodes a video file and reports, for each frame, the pixel format the codec really used and, for
// every component of it, how many *distinct* values that component holds. Those numbers are the point of
// the tool: a stream that was 16-bit at the source and comes back with at most 256 distinct levels lost
// everything below eight bits somewhere in the chain, whatever the codec calls itself, and a stream that
// carried a mask in its alpha comes back with no alpha channel at all when the format had none to give.
// It is how the recording modes of CyGameCaptureOBS were measured (see that plugin README).
//
// It links the FFmpeg libraries OBS already ships, so there is nothing extra to install; only the
// public headers are vendored, and the import libraries are generated from the installed OBS by
// Tools\GenerateFFmpegImportLibs.cmd.
#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace
{
	/**
	* The FFmpeg DLLs live inside the OBS installation, not next to this tool and not on the PATH. The
	* imports are delay loaded (see the CMake target), so pointing the loader at that folder here,
	* before the first FFmpeg call, is all it takes for them to resolve.
	*/
	bool PointLoaderAtObs(const wchar_t *override_directory)
	{
		if (override_directory != nullptr && *override_directory != 0)
			return SetDllDirectoryW(override_directory) != FALSE;

		wchar_t path[MAX_PATH] = {};
		DWORD size = sizeof(path);
		// OBS records where it was installed; fall back to the default location when it did not
		if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\OBS Studio", nullptr, RRF_RT_REG_SZ, nullptr, path, &size) != ERROR_SUCCESS)
		{
			if (GetEnvironmentVariableW(L"ProgramFiles", path, MAX_PATH) == 0)
				return false;
			wcsncat_s(path, L"\\obs-studio", _TRUNCATE);
		}
		wcsncat_s(path, L"\\bin\\64bit", _TRUNCATE);

		if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
			return false;
		return SetDllDirectoryW(path) != FALSE;
	}

	/** One sample of one component, normalised to 0..255 whatever the stored depth. */
	uint8_t SampleAsByte(const AVFrame *frame, const AVPixFmtDescriptor *desc, int component, int x, int y)
	{
		const AVComponentDescriptor &c = desc->comp[component];
		const bool chroma = component == 1 || component == 2;
		const int sample_x = chroma ? (x >> desc->log2_chroma_w) : x;
		const int sample_y = chroma ? (y >> desc->log2_chroma_h) : y;

		const uint8_t *const row = frame->data[c.plane] + static_cast<ptrdiff_t>(sample_y) * frame->linesize[c.plane];
		const uint8_t *const sample = row + static_cast<ptrdiff_t>(sample_x) * c.step + c.offset;
		const uint32_t value = c.depth > 8
			? static_cast<uint32_t>(sample[0] | (static_cast<uint32_t>(sample[1]) << 8))
			: static_cast<uint32_t>(sample[0]);
		return static_cast<uint8_t>(value >> (c.depth - 8));
	}

	/**
	* Writes one component of the frame as a grey-scale .bmp. Meant for looking at a channel that is
	* invisible otherwise -- the alpha of an isolated HUD, for instance, which is where its mask lives.
	*/
	bool DumpComponent(const AVFrame *frame, const AVPixFmtDescriptor *desc, int component, const std::string &path)
	{
		if (desc == nullptr || component >= desc->nb_components)
			return false;

		const int width = frame->width;
		const int height = frame->height;
		const int row_bytes = ((width * 3 + 3) / 4) * 4;
		const uint32_t pixels_size = static_cast<uint32_t>(row_bytes) * height;

		FILE *file = nullptr;
		if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
			return false;

		const uint32_t file_size = 54 + pixels_size;
		const uint8_t header[54] = {
			'B', 'M',
			static_cast<uint8_t>(file_size), static_cast<uint8_t>(file_size >> 8),
			static_cast<uint8_t>(file_size >> 16), static_cast<uint8_t>(file_size >> 24),
			0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
			static_cast<uint8_t>(width), static_cast<uint8_t>(width >> 8),
			static_cast<uint8_t>(width >> 16), static_cast<uint8_t>(width >> 24),
			static_cast<uint8_t>(height), static_cast<uint8_t>(height >> 8),
			static_cast<uint8_t>(height >> 16), static_cast<uint8_t>(height >> 24),
			1, 0, 24, 0, 0, 0, 0, 0,
			static_cast<uint8_t>(pixels_size), static_cast<uint8_t>(pixels_size >> 8),
			static_cast<uint8_t>(pixels_size >> 16), static_cast<uint8_t>(pixels_size >> 24),
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		};
		fwrite(header, 1, sizeof(header), file);

		std::vector<uint8_t> row(static_cast<size_t>(row_bytes), 0);
		for (int y = height - 1; y >= 0; --y)   // .bmp rows run bottom to top
		{
			for (int x = 0; x < width; ++x)
			{
				const uint8_t value = SampleAsByte(frame, desc, component, x, y);
				row[static_cast<size_t>(x) * 3 + 0] = value;
				row[static_cast<size_t>(x) * 3 + 1] = value;
				row[static_cast<size_t>(x) * 3 + 2] = value;
			}
			fwrite(row.data(), 1, row.size(), file);
		}
		fclose(file);
		return true;
	}

	/** Name of a component, so the report reads as R/G/B/A or Y/U/V/A rather than 0/1/2/3. */
	const char *ComponentName(const AVPixFmtDescriptor *desc, int component)
	{
		const bool rgb = desc != nullptr && (desc->flags & AV_PIX_FMT_FLAG_RGB) != 0;
		static const char *const rgb_names[4] = { "R", "G", "B", "A" };
		static const char *const yuv_names[4] = { "Y", "U", "V", "A" };
		return component < 4 ? (rgb ? rgb_names[component] : yuv_names[component]) : "?";
	}

	/**
	* One component of one frame.
	*
	* The descriptor drives everything, so packed and planar formats are read the same way: `plane`
	* says which buffer, `step` how far apart two horizontal samples are, `offset` where the first one
	* starts. Chroma planes of a subsampled format are smaller, hence the shifts.
	*/
	void ReportComponent(const AVFrame *frame, const AVPixFmtDescriptor *desc, int component)
	{
		const AVComponentDescriptor &c = desc->comp[component];
		const bool chroma = component == 1 || component == 2;
		const int width = chroma ? AV_CEIL_RSHIFT(frame->width, desc->log2_chroma_w) : frame->width;
		const int height = chroma ? AV_CEIL_RSHIFT(frame->height, desc->log2_chroma_h) : frame->height;
		const bool wide = c.depth > 8;

		std::set<uint32_t> levels;
		uint32_t minimum = 0xFFFFFFFF;
		uint32_t maximum = 0;
		long double sum = 0.0;
		uint64_t count = 0;

		for (int y = 0; y < height; ++y)
		{
			const uint8_t *const row = frame->data[c.plane] + static_cast<ptrdiff_t>(y) * frame->linesize[c.plane];
			for (int x = 0; x < width; ++x)
			{
				const uint8_t *const sample = row + static_cast<ptrdiff_t>(x) * c.step + c.offset;
				const uint32_t value = wide
					? static_cast<uint32_t>(sample[0] | (static_cast<uint32_t>(sample[1]) << 8))
					: static_cast<uint32_t>(sample[0]);
				levels.insert(value);
				minimum = value < minimum ? value : minimum;
				maximum = value > maximum ? value : maximum;
				sum += value;
				++count;
			}
		}

		const double scale = static_cast<double>((1u << c.depth) - 1);
		printf("      %s  depth=%2d  %4dx%-4d  distinct levels=%6zu  min=%5u max=%5u mean=%8.1f  (%.4f..%.4f)\n",
			ComponentName(desc, component), c.depth, width, height, levels.size(), minimum, maximum,
			static_cast<double>(sum / (count != 0 ? count : 1)), minimum / scale, maximum / scale);
	}

	void ReportFrame(const AVFrame *frame, int index)
	{
		const AVPixFmtDescriptor *const desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
		if (desc == nullptr)
		{
			printf("  frame %2d  unknown pixel format\n", index);
			return;
		}

		const bool has_alpha = (desc->flags & AV_PIX_FMT_FLAG_ALPHA) != 0;
		printf("  frame %2d  %s  %dx%d  %s  %d component(s)%s\n", index, desc->name, frame->width, frame->height,
			(desc->flags & AV_PIX_FMT_FLAG_PLANAR) != 0 ? "planar" : "packed", desc->nb_components,
			has_alpha ? ", with alpha" : ", NO ALPHA CHANNEL");

		for (int component = 0; component < desc->nb_components; ++component)
			ReportComponent(frame, desc, component);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: CyGameCaptureVideoProbe <file> [frames] [--dump <component> <folder>]\n\n"
			"Reports what really survived in a recording: the pixel format the codec used, its bit\n"
			"depth, and how many distinct values its first component holds. Set CYGC_OBS_BIN to use\n"
			"an obs-studio\\bin\\64bit folder other than the installed one.\n\n"
			"--dump writes one component of each frame as a grey-scale .bmp, which is how you look at a\n"
			"channel that is invisible otherwise: --dump 3 <folder> saves the alpha, where the mask of an\n"
			"isolated HUD lives.\n");
		return 1;
	}
	int wanted = 3;
	int dump_component = -1;
	std::string dump_directory;
	for (int i = 2; i < argc; ++i)
	{
		if (strcmp(argv[i], "--dump") == 0 && i + 2 < argc)
		{
			dump_component = atoi(argv[++i]);
			dump_directory = argv[++i];
		}
		else
		{
			wanted = atoi(argv[i]);
		}
	}
	if (wanted <= 0)
		wanted = 3;

	wchar_t override_directory[MAX_PATH] = {};
	GetEnvironmentVariableW(L"CYGC_OBS_BIN", override_directory, MAX_PATH);
	if (!PointLoaderAtObs(override_directory))
	{
		printf("ERROR: could not find the FFmpeg libraries of OBS Studio.\n"
			"       Set CYGC_OBS_BIN to its bin\\64bit folder.\n");
		return 1;
	}

	AVFormatContext *format_context = nullptr;
	if (avformat_open_input(&format_context, argv[1], nullptr, nullptr) < 0)
	{
		printf("ERROR: could not open %s\n", argv[1]);
		return 1;
	}
	if (avformat_find_stream_info(format_context, nullptr) < 0)
	{
		printf("ERROR: no stream info\n");
		return 1;
	}

	const AVCodec *codec = nullptr;
	const int stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
	if (stream_index < 0 || codec == nullptr)
	{
		printf("ERROR: no video stream\n");
		return 1;
	}

	AVStream *const stream = format_context->streams[stream_index];
	printf("file   : %s\n", argv[1]);
	printf("codec  : %s\n", codec->name);
	printf("stored : %s %dx%d\n", av_get_pix_fmt_name(static_cast<AVPixelFormat>(stream->codecpar->format)),
		stream->codecpar->width, stream->codecpar->height);
	printf("frames : %lld\n", static_cast<long long>(stream->nb_frames));

	AVCodecContext *context = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(context, stream->codecpar);
	if (avcodec_open2(context, codec, nullptr) < 0)
	{
		printf("ERROR: could not open the decoder\n");
		return 1;
	}

	AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	int decoded = 0;

	while (decoded < wanted && av_read_frame(format_context, packet) >= 0)
	{
		if (packet->stream_index == stream_index && avcodec_send_packet(context, packet) >= 0)
		{
			while (decoded < wanted && avcodec_receive_frame(context, frame) >= 0)
			{
				ReportFrame(frame, decoded);
				if (dump_component >= 0)
				{
					const AVPixFmtDescriptor *const desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
					const std::string path = dump_directory + "\\component" + std::to_string(dump_component) +
						"_frame" + std::to_string(decoded) + ".bmp";
					if (DumpComponent(frame, desc, dump_component, path))
						printf("      -> %s\n", path.c_str());
					else
						printf("      -> could not write %s (does the format have that component?)\n", path.c_str());
				}
				++decoded;
			}
		}
		av_packet_unref(packet);
	}

	printf("decoded: %d frame(s)\n", decoded);

	av_frame_free(&frame);
	av_packet_free(&packet);
	avcodec_free_context(&context);
	avformat_close_input(&format_context);
	return decoded > 0 ? 0 : 1;
}
