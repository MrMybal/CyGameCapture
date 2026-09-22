// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureOBS — the "CyGameCapture" OBS input source.
//
// It receives a Spout2 sender published by CyGameCaptureRS (ReShade add-on) or CyGameCaptureUE
// (Unreal plugin), and any other Spout sender when asked to.
//
// The video path never touches the CPU: Spout publishes a legacy DXGI shared handle in shared memory,
// OBS opens that very texture on its own D3D11 device with gs_texture_open_shared() and draws it.
// There is no copy, no readback and no format conversion on our side.
//
// Lifetime rules that matter here:
//   - the shared handle belongs to the sender: it becomes invalid the moment the sender resizes,
//     restarts or exits, so the published description is re-read every tick and the texture is
//     reopened as soon as anything about it changes;
//   - a sender that disappears is reported as "Capture source lost", never silently replaced by a
//     different one, unless the user explicitly asked for auto-connect.
#include "SpoutDiscovery.hpp"
#include "BufferRecorder.hpp"

#include <obs-module.h>

#include <CyGameCaptureCore/FrameSync.hpp>
#include <CyGameCaptureCore/SenderNaming.hpp>
#include <CyGameCaptureCore/StreamInfo.hpp>
#include <CyGameCaptureCore/Version.hpp>

#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace
{
	using namespace cygc::obs;

	constexpr const char *kSettingSender = "sender_name";
	constexpr const char *kSettingAutoConnect = "auto_connect";
	constexpr const char *kSettingShowAll = "show_all_senders";
	constexpr const char *kSettingHoldLastFrame = "hold_last_frame";
	constexpr const char *kSettingRecord = "record_buffer";
	constexpr const char *kSettingRecordFolder = "record_folder";
	constexpr const char *kSettingRecordEncoder = "record_encoder";
	constexpr const char *kSettingRecordContainer = "record_container";
	constexpr const char *kSettingRecordAudio = "record_audio";
	constexpr const char *kSettingRecordAutoStart = "record_autostart";
	constexpr const char *kSettingRecordMode = "record_mode";

	/** How often the sender table is re-read while nothing is connected (seconds). */
	constexpr float kReconnectInterval = 0.5f;

	struct CyGameCaptureSource
	{
		obs_source_t *source = nullptr;

		/**
		 * OBS calls `update`, `get_properties`, `get_width` and `get_height` from the UI thread while
		 * `video_tick` and `video_render` run on the graphics thread. Everything below is shared between
		 * them: without this lock, changing the source in the properties could destroy the texture, or
		 * reassign the sender name, while a frame is being drawn.
		 *
		 * Lock order, always: OBS' graphics context first (obs_enter_graphics), this mutex second. Any
		 * code path that needs both takes them in that order, so the two can never deadlock against each
		 * other. Paths that only read the description (get_width / get_height) take this mutex alone,
		 * which is safe for an inner lock.
		 */
		std::mutex mutex;

		// Settings
		std::string requested_sender;   // empty + auto_connect = "first CyGameCapture sender"
		bool auto_connect = true;
		bool show_all_senders = false;
		bool hold_last_frame = true;

		// Connection state
		SenderAccess access;
		SenderDescription description;
		gs_texture_t *texture = nullptr;
		bool connected = false;
		float time_since_scan = 0.0f;
		std::string last_error;         // logged once, cleared on a successful connection

		// Producer frame index of the picture currently in the shared texture, read from the frame-sync
		// channel. Only meaningful for CyGameCapture senders; a foreign Spout sender publishes nothing.
		uint64_t last_sync_frame = 0;

		/**
		* True when this stream carries measurements rather than a picture -- a linearised depth buffer,
		* today. It changes how the frame must be drawn: OBS applies its sRGB transfer function on the
		* way into the mix, which is right for colours and destroys data. See SourceVideoRender.
		*/
		bool is_data_stream = false;

		/**
		* True when the stream carries a mask in its alpha channel -- an isolated HUD. Only the 8-bit
		* lossless mode keeps that channel: the hardware encoder and the 16-bit mix are both YUV and
		* have no alpha to put it in, so a take in those modes silently loses the cut-out.
		*/
		bool is_mask_stream = false;

		/**
		* Recording state, under its own lock: a take is started and stopped from the properties, while
		* the frame path only ever reads whether recording is on. Keeping it separate means starting a
		* take never blocks a frame being drawn.
		*/
		std::mutex recorder_mutex;
		StreamRecorder recorder;
		bool record_enabled = false;
		bool record_autostart = false;
		RecorderSettings record_settings;
	};

	/**
	* Every live CyGameCapture source. A take starts and stops all the enabled ones in one go, which is
	* what makes the resulting files line up: all their mixes are then driven by the same OBS tick.
	*/
	std::mutex &RegistryMutex()
	{
		static std::mutex mutex;
		return mutex;
	}

	std::set<CyGameCaptureSource *> &Registry()
	{
		static std::set<CyGameCaptureSource *> registry;
		return registry;
	}

	/** Default take folder when the user has not chosen one. */
	std::string DefaultRecordFolder()
	{
		const char *const profile = getenv("USERPROFILE");
		return profile != nullptr ? std::string(profile) + "\\Videos\\CyGameCapture" : std::string("C:\\CyGameCapture");
	}

	/** Time stamp shared by every file of one take, so they are obviously the same recording. */
	std::string TakeStamp()
	{
		SYSTEMTIME now = {};
		GetLocalTime(&now);
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u-%02u-%02u", now.wYear, now.wMonth, now.wDay,
			now.wHour, now.wMinute, now.wSecond);
		return buffer;
	}

	/**
	* Starts every source whose "Record this buffer" is on. They are started back to back inside one
	* call, so the first frame each output receives comes from the same OBS tick.
	*/
	int StartTake(std::string &error)
	{
		const std::string stamp = TakeStamp();
		std::vector<CyGameCaptureSource *> targets;
		{
			std::lock_guard<std::mutex> lock(RegistryMutex());
			targets.assign(Registry().begin(), Registry().end());
		}

		int started = 0;
		for (CyGameCaptureSource *const ctx : targets)
		{
			std::string name;
			{
				std::lock_guard<std::mutex> lock(ctx->mutex);
				if (!ctx->connected)
					continue;
				name = ctx->description.name;
			}

			std::lock_guard<std::mutex> lock(ctx->recorder_mutex);
			if (!ctx->record_enabled || ctx->recorder.IsActive())
				continue;

			// The OBS source name is part of the file name, not only the sender: two sources may well
			// record the same buffer with different settings, and without it they would collide on one
			// path and silently overwrite each other.
			const char *const source_name = obs_source_get_name(ctx->source);
			std::string stem = stamp + " ";
			if (source_name != nullptr && *source_name != '\0')
				stem += std::string(source_name) + " - ";
			stem += std::string(cygc::SenderDisplayName(name));
			// Say so before the take rather than after: the mask is gone by the time the file exists
			if (ctx->is_mask_stream && ctx->record_settings.mode != RecordMode::LosslessRgb)
			{
				blog(LOG_WARNING, "[CyGameCapture] '%s' carries its mask in the alpha channel, and the "
					"chosen encoding has no alpha channel to keep it in. Only \"Lossless, 8-bit RGB\" "
					"preserves an isolated HUD.", name.c_str());
			}

			std::string start_error;
			if (ctx->recorder.Start(ctx->source, stem, ctx->record_settings, start_error))
			{
				++started;
			}
			else
			{
				blog(LOG_WARNING, "[CyGameCapture] Could not record '%s': %s", name.c_str(), start_error.c_str());
				if (error.empty())
					error = start_error;
			}
		}
		return started;
	}

	int StopTake()
	{
		std::vector<CyGameCaptureSource *> targets;
		{
			std::lock_guard<std::mutex> lock(RegistryMutex());
			targets.assign(Registry().begin(), Registry().end());
		}

		int stopped = 0;
		for (CyGameCaptureSource *const ctx : targets)
		{
			std::lock_guard<std::mutex> lock(ctx->recorder_mutex);
			if (!ctx->recorder.IsActive())
				continue;
			ctx->recorder.Stop();
			++stopped;
		}
		return stopped;
	}

	/**
	* Unattended takes: when at least one buffer asked for it, the take starts on its own as soon as
	* every buffer that is part of it has a picture. Used for recording a whole session without having
	* to click anything, and it only ever fires once per OBS run.
	*/
	void MaybeAutoStartTake()
	{
		static bool already_started = false;
		if (already_started)
			return;

		bool wanted = false;
		bool all_ready = true;
		{
			std::lock_guard<std::mutex> lock(RegistryMutex());
			for (CyGameCaptureSource *const ctx : Registry())
			{
				bool enabled = false;
				{
					std::lock_guard<std::mutex> recorder_lock(ctx->recorder_mutex);
					enabled = ctx->record_enabled;
					wanted |= ctx->record_autostart && enabled;
				}
				if (!enabled)
					continue;
				std::lock_guard<std::mutex> state_lock(ctx->mutex);
				all_ready &= ctx->connected;
			}
		}
		if (!wanted || !all_ready)
			return;

		already_started = true;
		std::string error;
		const int started = StartTake(error);
		blog(LOG_INFO, "[CyGameCapture] Take started automatically on %d buffer(s)%s%s", started,
			error.empty() ? "" : " - ", error.c_str());
	}

	/** How many sources are writing a file right now. */
	int ActiveRecordingCount()
	{
		std::lock_guard<std::mutex> lock(RegistryMutex());
		int count = 0;
		for (CyGameCaptureSource *const ctx : Registry())
		{
			std::lock_guard<std::mutex> recorder_lock(ctx->recorder_mutex);
			if (ctx->recorder.IsActive())
				++count;
		}
		return count;
	}

	/**
	* Largest gap, in producer frames, between the streams being recorded right now. Zero means every
	* file is receiving the same game frame; anything else is drift the editor would have to fix by hand.
	*/
	uint64_t RecordingFrameSpread()
	{
		uint64_t lowest = UINT64_MAX;
		uint64_t highest = 0;

		std::lock_guard<std::mutex> lock(RegistryMutex());
		for (CyGameCaptureSource *const ctx : Registry())
		{
			{
				std::lock_guard<std::mutex> recorder_lock(ctx->recorder_mutex);
				if (!ctx->recorder.IsActive())
					continue;
			}
			std::lock_guard<std::mutex> state_lock(ctx->mutex);
			if (ctx->last_sync_frame == 0)
				continue;   // a sender that publishes no frame index cannot be checked
			lowest = std::min(lowest, ctx->last_sync_frame);
			highest = std::max(highest, ctx->last_sync_frame);
		}
		return lowest == UINT64_MAX ? 0 : highest - lowest;
	}

	// ---------------------------------------------------------------------------------------------
	/** Caller holds the graphics context and ctx->mutex (see the lock order note above). */
	void CloseTexture(CyGameCaptureSource *ctx)
	{
		if (ctx->texture == nullptr)
			return;
		gs_texture_destroy(ctx->texture);
		ctx->texture = nullptr;
	}

	void Disconnect(CyGameCaptureSource *ctx, const char *reason)
	{
		if (ctx->connected)
			blog(LOG_INFO, "[CyGameCapture] Capture source lost: '%s' (%s)", ctx->description.name.c_str(), reason);
		CloseTexture(ctx);
		ctx->access.Close();
		ctx->connected = false;
		ctx->description = SenderDescription();
	}

	/**
	 * Opens the shared texture described by `description`. Returns false and fills last_error on failure.
	 * Caller holds the graphics context and ctx->mutex.
	 */
	bool OpenTexture(CyGameCaptureSource *ctx, const SenderDescription &description)
	{
		// Spout stores the legacy DXGI share handle; gs_texture_open_shared takes exactly that, as 32 bits.
		const uint32_t handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(description.share_handle));

		gs_texture_t *const texture = gs_texture_open_shared(handle);
		if (texture == nullptr)
		{
			ctx->last_error = "OBS could not open the shared texture of '" + description.name + "' (format " +
				std::to_string(description.dxgi_format) + " may be unsupported by OBS)";
			return false;
		}

		CloseTexture(ctx);
		ctx->texture = texture;
		return true;
	}

	bool Connect(CyGameCaptureSource *ctx, const SenderDescription &description)
	{
		if (!description.IsValid())
			return false;
		if (!OpenTexture(ctx, description))
		{
			blog(LOG_WARNING, "[CyGameCapture] %s", ctx->last_error.c_str());
			return false;
		}

		ctx->access.Open(description.name);
		ctx->description = description;
		ctx->connected = true;
		ctx->last_error.clear();

		// The producer says what the stream is in the frame-sync channel; the 16-bit format is the
		// fallback for a sender that publishes nothing, since that is what the depth stream uses.
		cygc::FrameSyncRecord sync;
		const bool published = cygc::ReadFrameSync(description.name, sync);
		ctx->is_data_stream = published
			? sync.stream_kind == static_cast<uint32_t>(cygc::StreamKind::Depth)
			: description.dxgi_format == 11;   // DXGI_FORMAT_R16G16B16A16_UNORM
		ctx->is_mask_stream = published && sync.stream_kind == static_cast<uint32_t>(cygc::StreamKind::UI);

		blog(LOG_INFO, "[CyGameCapture] Connected to '%s' (%s) %ux%u DXGI format %u, shared handle 0x%p",
			description.name.c_str(), cygc::SenderOriginName(description.name),
			description.width, description.height, description.dxgi_format, description.share_handle);
		return true;
	}

	/** Sender the source should be attached to right now, or an empty description when there is none. */
	SenderDescription ResolveTarget(const CyGameCaptureSource *ctx)
	{
		SenderDescription description;

		if (!ctx->requested_sender.empty())
		{
			if (GetSenderDescription(ctx->requested_sender, description))
				return description;
			if (!ctx->auto_connect)
				return SenderDescription();
		}

		if (!ctx->auto_connect)
			return SenderDescription();

		// Auto-connect: prefer a CyGameCapture sender, fall back to Spout's active sender only when the
		// user allowed every sender to be listed.
		for (const SenderDescription &candidate : EnumerateSenders())
		{
			if (cygc::IsCyGameCaptureSender(candidate.name) && candidate.IsValid())
				return candidate;
		}
		if (ctx->show_all_senders)
		{
			std::string active;
			if (GetActiveSenderName(active) && GetSenderDescription(active, description))
				return description;
		}
		return SenderDescription();
	}

	// ---------------------------------------------------------------------------------------------
	const char *SourceGetName(void *)
	{
		return obs_module_text("CyGameCapture");
	}

	void SourceUpdate(void *data, obs_data_t *settings)
	{
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);

		const char *const sender = obs_data_get_string(settings, kSettingSender);
		const std::string requested = sender != nullptr ? sender : "";
		const bool auto_connect = obs_data_get_bool(settings, kSettingAutoConnect);

		// Graphics first, then our mutex: Disconnect() below destroys a GPU texture
		obs_enter_graphics();
		{
			std::lock_guard<std::mutex> lock(ctx->mutex);
			const bool target_changed = requested != ctx->requested_sender || auto_connect != ctx->auto_connect;

			ctx->requested_sender = requested;
			ctx->auto_connect = auto_connect;
			ctx->show_all_senders = obs_data_get_bool(settings, kSettingShowAll);
			ctx->hold_last_frame = obs_data_get_bool(settings, kSettingHoldLastFrame);

			if (target_changed)
			{
				Disconnect(ctx, "source changed in the properties");
				ctx->time_since_scan = kReconnectInterval;   // reconnect on the next tick
			}
		}
		obs_leave_graphics();

		// Recording settings live under their own lock and never touch the GPU
		{
			std::lock_guard<std::mutex> lock(ctx->recorder_mutex);
			ctx->record_enabled = obs_data_get_bool(settings, kSettingRecord);

			const char *const folder = obs_data_get_string(settings, kSettingRecordFolder);
			ctx->record_settings.output_folder = (folder != nullptr && *folder != '\0') ? folder : DefaultRecordFolder();

			const char *const encoder = obs_data_get_string(settings, kSettingRecordEncoder);
			ctx->record_settings.encoder_id = encoder != nullptr ? encoder : "";

			const char *const container = obs_data_get_string(settings, kSettingRecordContainer);
			ctx->record_settings.container = (container != nullptr && *container != '\0') ? container : "mkv";

			ctx->record_settings.mode = static_cast<RecordMode>(obs_data_get_int(settings, kSettingRecordMode));

			const long long track = obs_data_get_int(settings, kSettingRecordAudio);
			ctx->record_settings.audio_track = static_cast<size_t>(track > 0 ? track - 1 : 0);
			ctx->record_autostart = obs_data_get_bool(settings, kSettingRecordAutoStart);
		}
	}

	void *SourceCreate(obs_data_t *settings, obs_source_t *source)
	{
		auto *const ctx = new CyGameCaptureSource();
		ctx->source = source;
		SourceUpdate(ctx, settings);
		{
			std::lock_guard<std::mutex> lock(RegistryMutex());
			Registry().insert(ctx);
		}
		return ctx;
	}

	void SourceDestroy(void *data)
	{
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);
		{
			std::lock_guard<std::mutex> lock(RegistryMutex());
			Registry().erase(ctx);
		}
		{
			// A take in progress on this buffer ends with the source; the file stays where it was written
			std::lock_guard<std::mutex> lock(ctx->recorder_mutex);
			ctx->recorder.Stop();
		}
		obs_enter_graphics();
		{
			// OBS has already stopped rendering this source, but keep the discipline of the other callbacks
			std::lock_guard<std::mutex> lock(ctx->mutex);
			Disconnect(ctx, "source removed");
		}
		obs_leave_graphics();
		delete ctx;
	}

	uint32_t SourceGetWidth(void *data)
	{
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);
		std::lock_guard<std::mutex> lock(ctx->mutex);
		return ctx->description.width;
	}

	uint32_t SourceGetHeight(void *data)
	{
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);
		std::lock_guard<std::mutex> lock(ctx->mutex);
		return ctx->description.height;
	}

	void SourceGetDefaults(obs_data_t *settings)
	{
		obs_data_set_default_string(settings, kSettingSender, "");
		obs_data_set_default_bool(settings, kSettingAutoConnect, true);
		obs_data_set_default_bool(settings, kSettingShowAll, false);
		obs_data_set_default_bool(settings, kSettingHoldLastFrame, true);
		obs_data_set_default_bool(settings, kSettingRecord, false);
		obs_data_set_default_string(settings, kSettingRecordFolder, DefaultRecordFolder().c_str());
		obs_data_set_default_string(settings, kSettingRecordEncoder, DefaultVideoEncoderId().c_str());
		obs_data_set_default_string(settings, kSettingRecordContainer, "mkv");
		obs_data_set_default_int(settings, kSettingRecordAudio, 1);
		obs_data_set_default_int(settings, kSettingRecordMode, static_cast<long long>(RecordMode::Hardware));
		obs_data_set_default_bool(settings, kSettingRecordAutoStart, false);
	}

	void SourceVideoTick(void *data, float seconds)
	{
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);

		// Same order as everywhere else; on the graphics thread this is a cheap recursive acquire
		obs_enter_graphics();
		struct LeaveGraphics { ~LeaveGraphics() { obs_leave_graphics(); } } leave_graphics;   // released last
		std::unique_lock<std::mutex> lock(ctx->mutex);

		/**
		* MaybeAutoStartTake() below walks every source and takes their locks, this one included, so the
		* per-source lock has to be gone before it runs. This little guard makes that explicit.
		*/
		struct ReleaseGuard
		{
			std::unique_lock<std::mutex> &held;
			void Release() { if (held.owns_lock()) held.unlock(); }
		} leave_graphics_now{ lock };

		if (ctx->connected)
		{
			// The description lives in shared memory and is rewritten by the sender on every resize,
			// format change or restart: the handle held here is only valid while it stays identical.
			SenderDescription current;
			if (!GetSenderDescription(ctx->description.name, current))
			{
				Disconnect(ctx, "sender closed");
			}
			else if (!current.SameTexture(ctx->description))
			{
				blog(LOG_INFO, "[CyGameCapture] '%s' changed: %ux%u format %u -> %ux%u format %u, reopening",
					current.name.c_str(), ctx->description.width, ctx->description.height, ctx->description.dxgi_format,
					current.width, current.height, current.dxgi_format);
				if (OpenTexture(ctx, current))
					ctx->description = current;
				else
					Disconnect(ctx, "shared texture could not be reopened");
			}
		}

		if (ctx->connected)
		{
			// Which game frame is in the shared texture right now. Producers publish it next to the
			// sender; a foreign Spout sender publishes nothing and simply cannot be checked.
			cygc::FrameSyncRecord sync;
			ctx->last_sync_frame = cygc::ReadFrameSync(ctx->description.name, sync) ? sync.frame_index : 0;
			leave_graphics_now.Release();
			MaybeAutoStartTake();
			return;
		}
		ctx->last_sync_frame = 0;

		ctx->time_since_scan += seconds;
		if (ctx->time_since_scan < kReconnectInterval)
			return;
		ctx->time_since_scan = 0.0f;

		const SenderDescription target = ResolveTarget(ctx);
		if (target.IsValid())
			Connect(ctx, target);
	}

	void SourceVideoRender(void *data, gs_effect_t *)
	{
		// `effect` is OBS' default "Draw" effect, already begun: obs_source_draw() picks it up itself.
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);
		std::lock_guard<std::mutex> lock(ctx->mutex);
		if (ctx->texture == nullptr)
			return;

		// Spout's named access mutex: the sender holds it while it publishes a new description, so taking
		// it here keeps a resize from happening between the tick above and this draw. A sender that holds
		// it too long simply costs us one frame - the previous picture stays on screen.
		const bool acquired = ctx->access.BeginAccess();
		if (!acquired && !ctx->hold_last_frame)
			return;

		/**
		* A depth buffer holds distances, not colours. OBS' mixes are sRGB aware: because this source
		* declares OBS_SOURCE_SRGB, obs_source_draw() binds the texture as sRGB and enables sRGB on the
		* framebuffer, so the value written into the mix is the sRGB *encoding* of the sampled value. For
		* a colour buffer that round trip cancels out (decode on read, encode on write) and keeps
		* scaling and blending correct. For a 16-bit data texture there is no sRGB view to decode with,
		* so only the encode happens and every value comes out gamma-curved -- measured on a recording:
		* a depth of 0.0257 arrived as 0.174, which is exactly sRGB(0.0257).
		*
		* Turning linear sRGB off for the draw makes obs_source_draw() bind the plain view and leave the
		* framebuffer alone, so the numbers reach the mix untouched.
		*/
		const bool previous_linear_srgb = gs_get_linear_srgb();
		if (ctx->is_data_stream)
			gs_set_linear_srgb(false);

		obs_source_draw(ctx->texture, 0, 0, 0, 0, false);

		if (ctx->is_data_stream)
			gs_set_linear_srgb(previous_linear_srgb);

		if (acquired)
			ctx->access.EndAccess();
	}

	// ---------------------------------------------------------------------------------------------
	bool StartTakeClicked(obs_properties_t *, obs_property_t *, void *)
	{
		std::string error;
		const int started = StartTake(error);
		if (started == 0)
		{
			blog(LOG_WARNING, "[CyGameCapture] Nothing to record: %s",
				error.empty() ? "no connected buffer has \"Record this buffer\" enabled" : error.c_str());
		}
		else
		{
			blog(LOG_INFO, "[CyGameCapture] Take started on %d buffer(s)", started);
		}
		return true;   // refresh the properties so the status line shows the take
	}

	bool StopTakeClicked(obs_properties_t *, obs_property_t *, void *)
	{
		const int stopped = StopTake();
		blog(LOG_INFO, "[CyGameCapture] Take stopped (%d file(s))", stopped);
		return true;
	}

	bool RefreshClicked(obs_properties_t *props, obs_property_t *, void *data);

	/**
	* Fills the sender drop-down. It takes plain values rather than the source, so it can be called both
	* from the live source and from the settings currently being edited, and needs no lock of its own.
	*/
	void PopulateSenderList(obs_property_t *list, const std::string &requested_sender, bool show_all)
	{
		obs_property_list_clear(list);
		obs_property_list_add_string(list, obs_module_text("Sender.Automatic"), "");

		for (const SenderDescription &sender : EnumerateSenders())
		{
			const bool is_ours = cygc::IsCyGameCaptureSender(sender.name);
			if (!is_ours && !show_all)
				continue;

			const std::string label = std::string(cygc::SenderDisplayName(sender.name)) + "  [" +
				cygc::SenderOriginName(sender.name) + "]  " + std::to_string(sender.width) + "x" + std::to_string(sender.height);
			obs_property_list_add_string(list, label.c_str(), sender.name.c_str());
		}

		// A sender chosen earlier that is not running right now must stay selectable, otherwise OBS would
		// silently drop the setting and reconnecting later would be impossible.
		if (!requested_sender.empty())
		{
			bool present = false;
			for (size_t i = 0; i < obs_property_list_item_count(list); ++i)
			{
				const char *const value = obs_property_list_item_string(list, i);
				if (value != nullptr && requested_sender == value)
				{
					present = true;
					break;
				}
			}
			if (!present)
			{
				const std::string label = std::string(cygc::SenderDisplayName(requested_sender)) + "  " + obs_module_text("Sender.Offline");
				obs_property_list_add_string(list, label.c_str(), requested_sender.c_str());
			}
		}
	}

	bool ShowAllModified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
	{
		obs_property_t *const list = obs_properties_get(props, kSettingSender);
		if (list == nullptr)
			return false;

		// Rebuilt from the settings being edited, not from the live source state
		const char *const current = obs_data_get_string(settings, kSettingSender);
		PopulateSenderList(list, current != nullptr ? current : "", obs_data_get_bool(settings, kSettingShowAll));
		return true;
	}

	obs_properties_t *SourceGetProperties(void *data)
	{
		auto *const ctx = static_cast<CyGameCaptureSource *>(data);
		obs_properties_t *const props = obs_properties_create();

		// Everything that needs a lock is read once, here, and the locks are released before the take
		// counters below are consulted: those walk every source and would otherwise take this very mutex
		// a second time on the same thread.
		std::string requested_sender;
		std::string connected_name;
		std::string last_error;
		uint32_t connected_width = 0;
		uint32_t connected_height = 0;
		double sender_fps = 0.0;
		bool show_all = false;
		bool connected = false;
		if (ctx != nullptr)
		{
			std::lock_guard<std::mutex> lock(ctx->mutex);
			requested_sender = ctx->requested_sender;
			show_all = ctx->show_all_senders;
			connected = ctx->connected;
			connected_name = ctx->description.name;
			connected_width = ctx->description.width;
			connected_height = ctx->description.height;
			sender_fps = ctx->access.SenderFps();
			last_error = ctx->last_error;
		}

		std::string take_path;
		int take_frames = 0;
		int take_skipped = 0;
		bool take_active = false;
		if (ctx != nullptr)
		{
			std::lock_guard<std::mutex> lock(ctx->recorder_mutex);
			take_active = ctx->recorder.IsActive();
			take_path = ctx->recorder.Path();
			take_frames = ctx->recorder.TotalFrames();
			take_skipped = ctx->recorder.SkippedFrames();
		}

		obs_property_t *const list = obs_properties_add_list(props, kSettingSender, obs_module_text("Sender"),
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_set_long_description(list, obs_module_text("Sender.Description"));
		PopulateSenderList(list, requested_sender, show_all);

		obs_properties_add_button(props, "refresh", obs_module_text("Refresh"), RefreshClicked);

		obs_property_t *const show_all_property = obs_properties_add_bool(props, kSettingShowAll, obs_module_text("ShowAll"));
		obs_property_set_long_description(show_all_property, obs_module_text("ShowAll.Description"));
		obs_property_set_modified_callback(show_all_property, ShowAllModified);

		obs_property_t *const auto_connect = obs_properties_add_bool(props, kSettingAutoConnect, obs_module_text("AutoConnect"));
		obs_property_set_long_description(auto_connect, obs_module_text("AutoConnect.Description"));

		obs_property_t *const hold = obs_properties_add_bool(props, kSettingHoldLastFrame, obs_module_text("HoldLastFrame"));
		obs_property_set_long_description(hold, obs_module_text("HoldLastFrame.Description"));

		// --- recording ---------------------------------------------------------------------------
		obs_property_t *const record = obs_properties_add_bool(props, kSettingRecord, obs_module_text("Record"));
		obs_property_set_long_description(record, obs_module_text("Record.Description"));

		obs_properties_add_path(props, kSettingRecordFolder, obs_module_text("Record.Folder"),
			OBS_PATH_DIRECTORY, nullptr, nullptr);

		obs_property_t *const encoder = obs_properties_add_list(props, kSettingRecordEncoder,
			obs_module_text("Record.Encoder"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_set_long_description(encoder, obs_module_text("Record.Encoder.Description"));
		for (const std::pair<std::string, std::string> &available : AvailableVideoEncoders())
			obs_property_list_add_string(encoder, available.second.c_str(), available.first.c_str());

		obs_property_t *const mode = obs_properties_add_list(props, kSettingRecordMode,
			obs_module_text("Record.Mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
		obs_property_set_long_description(mode, obs_module_text("Record.Mode.Description"));
		obs_property_list_add_int(mode, obs_module_text("Record.Mode.Hardware"), static_cast<long long>(RecordMode::Hardware));
		obs_property_list_add_int(mode, obs_module_text("Record.Mode.LosslessRgb"), static_cast<long long>(RecordMode::LosslessRgb));
		obs_property_list_add_int(mode, obs_module_text("Record.Mode.Lossless16"), static_cast<long long>(RecordMode::Lossless16));

		obs_property_t *const container = obs_properties_add_list(props, kSettingRecordContainer,
			obs_module_text("Record.Container"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(container, "Matroska (.mkv)", "mkv");
		obs_property_list_add_string(container, "MP4 (.mp4)", "mp4");
		obs_property_set_long_description(container, obs_module_text("Record.Container.Description"));

		obs_property_t *const record_audio = obs_properties_add_list(props, kSettingRecordAudio,
			obs_module_text("Record.Audio"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
		obs_property_set_long_description(record_audio, obs_module_text("Record.Audio.Description"));
		for (int track = 1; track <= 6; ++track)
			obs_property_list_add_int(record_audio, ("Track " + std::to_string(track)).c_str(), track);

		obs_property_t *const autostart = obs_properties_add_bool(props, kSettingRecordAutoStart, obs_module_text("Record.AutoStart"));
		obs_property_set_long_description(autostart, obs_module_text("Record.AutoStart.Description"));

		obs_properties_add_button(props, "start_take", obs_module_text("Record.Start"), StartTakeClicked);
		obs_properties_add_button(props, "stop_take", obs_module_text("Record.Stop"), StopTakeClicked);

		// --- status ------------------------------------------------------------------------------
		std::string status;
		if (connected)
		{
			status = std::string(obs_module_text("Status.Connected")) + ": " + connected_name + "  " +
				std::to_string(connected_width) + "x" + std::to_string(connected_height);
			if (sender_fps > 0.0)
				status += "  " + std::to_string(static_cast<int>(sender_fps + 0.5)) + " fps";
		}
		else if (!last_error.empty())
		{
			status = last_error;
		}
		else
		{
			status = obs_module_text("Status.Waiting");
		}
		status += "\n" + std::to_string(ActiveSenderCount()) + " / " + std::to_string(MaxSenders()) + " " + obs_module_text("Status.SenderSlots");

		const int recording = ActiveRecordingCount();
		if (recording > 0)
		{
			status += "\n" + std::string(obs_module_text("Record.Status.Active")) + ": " + std::to_string(recording);

			// Gap between the streams of the take, in producer frames. 0 means they are frame for frame.
			status += "  |  " + std::string(obs_module_text("Record.Status.Drift")) + ": " + std::to_string(RecordingFrameSpread());
		}
		if (take_active)
		{
			status += "\n" + take_path + "  (" + std::to_string(take_frames) + " frames";
			if (take_skipped > 0)
				status += ", " + std::to_string(take_skipped) + " skipped";
			status += ")";
		}

		// OBS_TEXT_INFO shows the property's *description*, so the status itself is the description here.
		obs_properties_add_text(props, "status", status.c_str(), OBS_TEXT_INFO);

		// Copyright, licence, absence of warranty and where the source is
		const std::string about = std::string("CyGameCapture ") + cygc::kVersionString + " - " + cygc::kCopyrightString +
		                          "\n" + obs_module_text("About.Licence") + "\n" + cygc::kProjectUrl;
		obs_properties_add_text(props, "about", about.c_str(), OBS_TEXT_INFO);

		return props;
	}
	bool RefreshClicked(obs_properties_t *props, obs_property_t *, void *data)
	{
		obs_property_t *const list = obs_properties_get(props, kSettingSender);
		if (list == nullptr)
			return false;

		auto *const ctx = static_cast<CyGameCaptureSource *>(data);
		std::string requested_sender;
		bool show_all = false;
		if (ctx != nullptr)
		{
			std::lock_guard<std::mutex> lock(ctx->mutex);
			requested_sender = ctx->requested_sender;
			show_all = ctx->show_all_senders;
		}
		PopulateSenderList(list, requested_sender, show_all);
		return true;
	}
}

obs_source_info cygc_capture_source_info = {};

void cygc_register_capture_source()
{
	cygc_capture_source_info.id = "cygamecapture_source";
	cygc_capture_source_info.type = OBS_SOURCE_TYPE_INPUT;
	// No OBS_SOURCE_CUSTOM_DRAW on purpose: it makes OBS call video_render outside any effect pass, and
	// obs_source_draw() then finds no active effect and draws nothing (the source shows up black). Without
	// the flag OBS wraps the callback in its default "Draw" technique, which is exactly what is needed.
	// OBS_SOURCE_SRGB: obs_source_draw() binds the texture sRGB-aware, so the source is colour correct.
	// OBS_SOURCE_DO_NOT_DUPLICATE: duplicating would open the same shared handle twice for nothing.
	cygc_capture_source_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB | OBS_SOURCE_DO_NOT_DUPLICATE;
	cygc_capture_source_info.get_name = SourceGetName;
	cygc_capture_source_info.create = SourceCreate;
	cygc_capture_source_info.destroy = SourceDestroy;
	cygc_capture_source_info.get_width = SourceGetWidth;
	cygc_capture_source_info.get_height = SourceGetHeight;
	cygc_capture_source_info.get_defaults = SourceGetDefaults;
	cygc_capture_source_info.get_properties = SourceGetProperties;
	cygc_capture_source_info.update = SourceUpdate;
	cygc_capture_source_info.video_tick = SourceVideoTick;
	cygc_capture_source_info.video_render = SourceVideoRender;
	cygc_capture_source_info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;

	obs_register_source(&cygc_capture_source_info);
}
