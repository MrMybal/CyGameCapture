// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "BufferRecorder.hpp"

#include <util/platform.h>

#include <algorithm>
#include <filesystem>

namespace cygc::obs
{
	namespace
	{
		/**
		 * Preference order. Hardware encoders first because several of these run at once: a 3-buffer take
		 * at 1080p60 is three simultaneous encodes, which x264 would not sustain next to a running game.
		 * The ids are the ones libobs registers; whichever is missing is simply skipped.
		 */
		constexpr const char *kPreferredEncoders[] = {
			"jim_hevc_nvenc",     // NVIDIA HEVC (10-bit capable)
			"jim_nvenc",          // NVIDIA H.264
			"h265_texture_amf",   // AMD HEVC
			"h264_texture_amf",   // AMD H.264
			"obs_qsv11",          // Intel QuickSync
			"ffmpeg_nvenc",       // older NVIDIA path
			"obs_x264",           // CPU fallback
		};

		bool IsEncoderRegistered(const char *id)
		{
			const char *registered = nullptr;
			for (size_t i = 0; obs_enum_encoder_types(i, &registered); ++i)
			{
				if (registered != nullptr && strcmp(registered, id) == 0)
					return obs_get_encoder_type(id) == OBS_ENCODER_VIDEO;
			}
			return false;
		}

		/** First registered audio encoder that can go in an mkv/mp4, or nullptr. */
		const char *DefaultAudioEncoderId()
		{
			static constexpr const char *kCandidates[] = { "ffmpeg_aac", "CoreAudio_AAC", "libfdk_aac", "ffmpeg_opus" };
			for (const char *id : kCandidates)
			{
				const char *registered = nullptr;
				for (size_t i = 0; obs_enum_encoder_types(i, &registered); ++i)
					if (registered != nullptr && strcmp(registered, id) == 0)
						return id;
			}
			return nullptr;
		}

		/** Spout sender names contain "::" and Windows paths do not accept ':'. */
		std::string SanitizeFileName(const std::string &name)
		{
			std::string result;
			result.reserve(name.size());
			for (size_t i = 0; i < name.size(); ++i)
			{
				const char c = name[i];
				if (c == ':')
				{
					// "A::B" becomes "A-B" rather than "A--B"
					if (!result.empty() && result.back() != '-')
						result.push_back('-');
					continue;
				}
				result.push_back(strchr("\\/*?\"<>|", c) != nullptr ? '_' : c);
			}
			while (!result.empty() && (result.back() == '-' || result.back() == ' ' || result.back() == '.'))
				result.pop_back();
			return result.empty() ? std::string("buffer") : result;
		}
	}

	std::vector<std::pair<std::string, std::string>> AvailableVideoEncoders()
	{
		std::vector<std::pair<std::string, std::string>> result;
		for (const char *id : kPreferredEncoders)
		{
			if (!IsEncoderRegistered(id))
				continue;
			const char *display = obs_encoder_get_display_name(id);
			result.emplace_back(id, display != nullptr ? display : id);
		}
		return result;
	}

	std::string DefaultVideoEncoderId()
	{
		const std::vector<std::pair<std::string, std::string>> encoders = AvailableVideoEncoders();
		return encoders.empty() ? std::string() : encoders.front().first;
	}

	// -------------------------------------------------------------------------------------------
	StreamRecorder::~StreamRecorder()
	{
		Stop();
	}

	bool StreamRecorder::Start(obs_source_t *source, const std::string &file_stem, const RecorderSettings &settings, std::string &error)
	{
		Stop();

		if (source == nullptr)
		{
			error = "No source to record";
			return false;
		}

		// The source has to be connected before anything else: the mix is sized from it
		const uint32_t width = obs_source_get_width(source);
		const uint32_t height = obs_source_get_height(source);
		if (width == 0 || height == 0)
		{
			error = "The source has no picture yet (is the sender running?)";
			return false;
		}

		const std::string encoder_id = settings.encoder_id.empty() ? DefaultVideoEncoderId() : settings.encoder_id;
		if (encoder_id.empty())
		{
			error = "No video encoder available in this OBS installation";
			return false;
		}

		std::error_code ec;
		std::filesystem::create_directories(settings.output_folder, ec);
		if (ec)
		{
			error = "Cannot create the output folder: " + settings.output_folder;
			return false;
		}
		path_ = (std::filesystem::path(settings.output_folder) / (SanitizeFileName(file_stem) + "." + settings.container)).string();

		const char *const name = obs_source_get_name(source);
		source_name_ = name != nullptr ? name : "";

		// --- its own video mix, at the buffer's native resolution -----------------------------------
		// The frame rate follows OBS' own so every recorded buffer shares one timeline; the rest is the
		// buffer's own geometry, so nothing is rescaled between the shared texture and the encoder.
		struct obs_video_info ovi = {};
		if (!obs_get_video_info(&ovi))
		{
			error = "OBS video is not running";
			return false;
		}
		ovi.base_width = width;
		ovi.base_height = height;
		ovi.output_width = width;
		ovi.output_height = height;

		switch (settings.mode)
		{
		case RecordMode::LosslessRgb:
			// No conversion at all: the mix stages its 8-bit BGRA render target and FFV1 takes BGRA
			// directly, so the file is bit for bit what was drawn.
			ovi.output_format = VIDEO_FORMAT_BGRA;
			ovi.gpu_conversion = false;
			ovi.range = VIDEO_RANGE_FULL;
			break;
		case RecordMode::Lossless16:
			// The only way to get more than eight bits out of a mix: these formats make libobs render
			// into RGBA16F instead of BGRA. The output is 16-bit 4:4:4, full range so the luma uses it all.
			ovi.output_format = VIDEO_FORMAT_P416;
			ovi.gpu_conversion = true;
			ovi.range = VIDEO_RANGE_FULL;
			break;
		case RecordMode::Hardware:
		default:
			break;   // whatever OBS itself is set to
		}

		view_ = obs_view_create();
		if (view_ == nullptr)
		{
			error = "obs_view_create failed";
			ReleaseAll();
			return false;
		}
		obs_view_set_source(view_, 0, source);

		video_ = obs_view_add2(view_, &ovi);
		if (video_ == nullptr)
		{
			error = "obs_view_add2 failed (unsupported mix format or resolution)";
			ReleaseAll();
			return false;
		}

		if (settings.mode != RecordMode::Hardware)
			return StartLossless(settings, error);

		// --- encoders --------------------------------------------------------------------------------
		obs_data_t *const video_settings = obs_data_create();
		if (settings.bitrate_kbps != 0)
			obs_data_set_int(video_settings, "bitrate", settings.bitrate_kbps);
		video_encoder_ = obs_video_encoder_create(encoder_id.c_str(), (source_name_ + " (CyGameCapture)").c_str(), video_settings, nullptr);
		obs_data_release(video_settings);
		if (video_encoder_ == nullptr)
		{
			error = "Could not create the video encoder '" + encoder_id + "'";
			ReleaseAll();
			return false;
		}
		obs_encoder_set_video(video_encoder_, video_);

		// Not optional: libobs refuses to start an audio+video output, which ffmpeg_muxer is, without an
		// audio encoder. See RecorderSettings::audio_track.
		const char *const audio_id = DefaultAudioEncoderId();
		if (audio_id == nullptr)
		{
			error = "No audio encoder available, and OBS' muxer cannot write a video-only file";
			ReleaseAll();
			return false;
		}
		audio_encoder_ = obs_audio_encoder_create(audio_id, (source_name_ + " (CyGameCapture audio)").c_str(),
			nullptr, settings.audio_track, nullptr);
		if (audio_encoder_ == nullptr)
		{
			error = std::string("Could not create the audio encoder '") + audio_id + "'";
			ReleaseAll();
			return false;
		}
		obs_encoder_set_audio(audio_encoder_, obs_get_audio());

		// --- output ----------------------------------------------------------------------------------
		obs_data_t *const output_settings = obs_data_create();
		obs_data_set_string(output_settings, "path", path_.c_str());
		output_ = obs_output_create("ffmpeg_muxer", (source_name_ + " (CyGameCapture recorder)").c_str(), output_settings, nullptr);
		obs_data_release(output_settings);
		if (output_ == nullptr)
		{
			error = "Could not create the ffmpeg_muxer output";
			ReleaseAll();
			return false;
		}

		obs_output_set_video_encoder(output_, video_encoder_);
		obs_output_set_audio_encoder(output_, audio_encoder_, 0);

		if (!obs_output_start(output_))
		{
			const char *const last = obs_output_get_last_error(output_);
			error = last != nullptr && *last != '\0' ? last : ("Could not start recording to " + path_);
			ReleaseAll();
			return false;
		}

		blog(LOG_INFO, "[CyGameCapture] Recording '%s' %ux%u with %s -> %s", source_name_.c_str(), width, height,
			encoder_id.c_str(), path_.c_str());
		return true;
	}

	/**
	* The lossless path uses OBS' raw "Custom Output (FFmpeg)" output rather than an encoder plus a
	* muxer: it takes the mix frames uncompressed (obs_output_set_media points it at this stream's own
	* mix) and hands them to FFV1, so nothing in between re-encodes or re-samples them.
	*/
	bool StreamRecorder::StartLossless(const RecorderSettings &settings, std::string &error)
	{
		obs_data_t *const output_settings = obs_data_create();
		obs_data_set_string(output_settings, "url", path_.c_str());
		obs_data_set_string(output_settings, "format_name", "matroska");
		obs_data_set_string(output_settings, "video_encoder", "ffv1");
		obs_data_set_string(output_settings, "audio_encoder", "aac");
		obs_data_set_int(output_settings, "video_bitrate", 0);      // FFV1 ignores it; it is always lossless
		obs_data_set_int(output_settings, "audio_bitrate", 128);
		obs_data_set_int(output_settings, "gop_size", 1);           // FFV1 is all-intra anyway
		obs_data_set_string(output_settings, "video_settings", "");
		obs_data_set_string(output_settings, "audio_settings", "");

		output_ = obs_output_create("ffmpeg_output", (source_name_ + " (CyGameCapture lossless)").c_str(), output_settings, nullptr);
		obs_data_release(output_settings);
		if (output_ == nullptr)
		{
			error = "Could not create the ffmpeg_output (custom FFmpeg output)";
			ReleaseAll();
			return false;
		}

		// A raw output reads its frames from the media set here, which is what lets each buffer have
		// its own mix; without this it would record OBS' main canvas instead.
		obs_output_set_media(output_, video_, obs_get_audio());
		obs_output_set_mixer(output_, settings.audio_track);

		if (!obs_output_start(output_))
		{
			const char *const last = obs_output_get_last_error(output_);
			error = last != nullptr && *last != '\0' ? last : ("Could not start the lossless recording to " + path_);
			ReleaseAll();
			return false;
		}

		blog(LOG_INFO, "[CyGameCapture] Recording '%s' losslessly with FFV1 (%s) -> %s", source_name_.c_str(),
			settings.mode == RecordMode::Lossless16 ? "16-bit 4:4:4 mix" : "8-bit BGRA mix, bit exact", path_.c_str());
		return true;
	}

	void StreamRecorder::Stop()
	{
		if (output_ != nullptr && obs_output_active(output_))
		{
			obs_output_stop(output_);
			blog(LOG_INFO, "[CyGameCapture] Stopped recording '%s' (%d frames) -> %s", source_name_.c_str(),
				obs_output_get_total_frames(output_), path_.c_str());
		}
		ReleaseAll();
	}

	void StreamRecorder::ReleaseAll()
	{
		if (output_ != nullptr)
		{
			obs_output_release(output_);
			output_ = nullptr;
		}
		if (video_encoder_ != nullptr)
		{
			obs_encoder_release(video_encoder_);
			video_encoder_ = nullptr;
		}
		if (audio_encoder_ != nullptr)
		{
			obs_encoder_release(audio_encoder_);
			audio_encoder_ = nullptr;
		}
		if (view_ != nullptr)
		{
			// Order matters: leave the render loop before dropping the source reference and the view
			if (video_ != nullptr)
			{
				obs_view_remove(view_);
				video_ = nullptr;
			}
			obs_view_set_source(view_, 0, nullptr);
			obs_view_destroy(view_);
			view_ = nullptr;
		}
	}

	bool StreamRecorder::IsActive() const
	{
		return output_ != nullptr && obs_output_active(output_);
	}

	int StreamRecorder::TotalFrames() const
	{
		return output_ != nullptr ? obs_output_get_total_frames(output_) : 0;
	}

	int StreamRecorder::SkippedFrames() const
	{
		return output_ != nullptr ? obs_output_get_frames_dropped(output_) : 0;
	}
}
