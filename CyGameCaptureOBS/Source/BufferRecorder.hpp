// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureOBS — recording several captured buffers at once, each into its own file.
//
// The idea
// --------
// A game frame contains more than one interesting picture: the scene before the HUD is composited, the
// final image, the HUD on its own, the depth buffer. CyGameCaptureRS publishes each of them as its own
// Spout sender and each one becomes a CyGameCapture source in OBS. This records N of those sources
// simultaneously, one file per source, frame for frame, so they can be laid on top of each other in an
// editor afterwards (HUD / no-HUD comparisons) or fed to a depth-based 3D conversion.
//
// How it is done
// --------------
// libobs can run more than one video mix. `obs_view_add2()` gives an independent `video_t` per recorded
// source, with its own resolution *and* its own colour format, so each buffer is recorded at its native
// size and in a format that suits it (NV12 + HEVC for colour, a full-range RGB mix for data buffers).
// Each mix gets its own encoder and its own `ffmpeg_muxer` output.
//
// Nothing here touches pixels on the CPU: the source draws a shared GPU texture into its mix, and OBS
// hands the mix straight to a hardware encoder.
//
// Alignment
// ---------
// Every mix is rendered in the same OBS graphics tick, so the Nth frame of every file is the same tick.
// What that does *not* guarantee is that the producer had published the same game frame into each shared
// texture at that moment. CyGameCaptureCore's frame-sync channel carries exactly that information, so
// the recorder reads it each tick, and reports the worst drift it saw over the take.
//
// This file deliberately uses libobs only, never the OBS front-end API, so the same engine can run
// inside the OBS plugin or in a headless tool linking libobs.
#pragma once

#include <obs.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cygc::obs
{
	/**
	* How a buffer is encoded.
	*
	* What "lossless" can and cannot mean here is decided by libobs, not by the codec. Every video mix
	* renders into one of two texture formats (see obs_init_textures in libobs/obs.c): 8-bit BGRA for
	* the ordinary formats, or RGBA16F for the 10/16-bit ones. There is no 16-bit *unorm* mix, so a
	* 16-bit stream such as the linearised depth cannot come through OBS untouched whatever the codec.
	*
	*   Hardware     NVENC / AMF / QuickSync into an ffmpeg_muxer file. Lossy, 8-bit, 4:2:0.
	*   LosslessRgb  BGRA mix straight into FFV1. Bit exact for 8-bit content, which is everything
	*                except depth: a colour buffer or an isolated HUD comes back identical.
	*   Lossless16   P416 mix (RGBA16F render target, 16-bit 4:4:4 output) into FFV1. Keeps roughly
	*                eleven bits of the sixteen, more near zero where half floats are dense -- which is
	*                where a linearised depth puts its near geometry.
	*/
	enum class RecordMode
	{
		Hardware,
		LosslessRgb,
		Lossless16,
	};

	/** How one buffer is written out. */
	struct RecorderSettings
	{
		RecordMode mode = RecordMode::Hardware;
		std::string output_folder;      // created if missing
		std::string encoder_id;         // empty = best hardware encoder available
		std::string container = "mkv";  // mkv survives a crash, mp4 does not
		/**
		 * Which OBS audio track feeds the file, 0-based. OBS' ffmpeg_muxer declares itself as an
		 * audio+video output, and libobs refuses to start such an output without an audio encoder, so
		 * every file carries one track; there is no video-only mode to offer. The cost is negligible
		 * (an AAC track is about 1 MB per minute) even on a pure data buffer such as depth.
		 */
		size_t audio_track = 0;
		uint32_t bitrate_kbps = 0;      // 0 = the encoder's own default / quality mode
	};

	/** The encoders this build can use, best first, as {id, display name}. */
	std::vector<std::pair<std::string, std::string>> AvailableVideoEncoders();

	/** First usable encoder id, or empty when libobs has none registered. */
	std::string DefaultVideoEncoderId();

	/**
	 * Records one obs source into its own file. Owns a view, a video mix, the encoders and the output;
	 * everything is released by Stop() or the destructor.
	 */
	class StreamRecorder
	{
	public:
		StreamRecorder() = default;
		~StreamRecorder();

		StreamRecorder(const StreamRecorder &) = delete;
		StreamRecorder &operator=(const StreamRecorder &) = delete;

		/**
		 * `file_stem` is the base file name without extension; the caller makes it unique.
		 * Returns false and fills `error` with something the user can act on.
		 */
		bool Start(obs_source_t *source, const std::string &file_stem, const RecorderSettings &settings, std::string &error);
		void Stop();

		bool IsActive() const;
		const std::string &Path() const { return path_; }
		const std::string &SourceName() const { return source_name_; }

		/** Frames the muxer has written so far, for the status line. */
		int TotalFrames() const;
		/** Frames the encoder had to drop (encoder overloaded / disk too slow). */
		int SkippedFrames() const;

	private:
		/** The RecordMode::Lossless* path, called by Start once the mix exists. */
		bool StartLossless(const RecorderSettings &settings, std::string &error);
		void ReleaseAll();

		obs_view_t *view_ = nullptr;
		video_t *video_ = nullptr;
		obs_encoder_t *video_encoder_ = nullptr;
		obs_encoder_t *audio_encoder_ = nullptr;
		obs_output_t *output_ = nullptr;
		std::string path_;
		std::string source_name_;
	};
}
