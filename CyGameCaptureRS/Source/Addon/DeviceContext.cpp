// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "DeviceContext.hpp"
#include "GpuPass/PassInput.hpp"
#include "Backends/D3D12/D3D11On12Bridge.hpp"
#include "Config.hpp"
#include "Log.hpp"
#include "ResourceTracker/FormatUtils.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		std::string g_sender_base_name = "CyGameCaptureRS::Game";
	}

	const std::string &SenderBaseName()
	{
		return g_sender_base_name;
	}
	void SetSenderBaseName(std::string name)
	{
		g_sender_base_name = std::move(name);
	}

	// Private constructor / destructor access for the static factory helpers
	struct DeviceContextAccess
	{
		static DeviceContext *New(device *device) { return new DeviceContext(device); }
		static void Delete(DeviceContext *ctx) { delete ctx; }
	};

	DeviceContext::DeviceContext(device *device) :
		gpu_device(device),
		backend(QueryBackendSupport(device)),
		tracker(std::make_unique<ResourceTracker>(device)),
		preview(std::make_unique<PreviewManager>(device)),
		snapshots(std::make_unique<FrameSnapshots>(device)),
		thumbnails(std::make_unique<ThumbnailGrid>(device)),
		logo(std::make_unique<LogoTexture>(device))
	{
		captures.push_back(std::make_unique<CaptureManager>(device, backend, SenderBaseName(), 1));
		captures.front()->SetTracker(tracker.get());
		captures.front()->SetSnapshots(snapshots.get());

		// Nothing the add-on allocates for itself belongs in the tracker: it is not part of the game's
		// frame, and ReShade reports a destruction synchronously, straight back into whoever frees it.
		thumbnails->SetResourceNotice(
			[this](resource res) { tracker->Forget(res); },
			[this](resource res) { tracker->Forget(res); });
		logo->SetResourceNotice([this](resource res) { tracker->Forget(res); });
		ai = std::make_unique<AiAssistant>(device, *tracker, *snapshots);
		snapshots->SetResourceNotice(
			[this](resource res) { tracker->Forget(res); },
			[this](resource res) { tracker->Forget(res); });
		// Every write is reported to the snapshots while it is recorded, gated by their own flag
		tracker->SetWriteObserver(
			[this](command_list *cmd_list, resource res, uint32_t count) { snapshots->OnWrite(cmd_list, res, count); },
			&snapshots->ActiveFlag());

		tracker->AddDestroyListener([this](resource resource, uint32_t id) {
			for (const std::unique_ptr<CaptureManager> &capture : captures)
				capture->OnResourceDestroyed(resource, id);
			if (snapshots)
				snapshots->OnResourceDestroyed(resource);
		});

		flush_after_copy = config::GetBool("FlushAfterCopy", false);

		view_toggle_key = static_cast<uint32_t>(config::GetInt("ViewToggleKey", 0x79 /* VK_F10 */));

		log::Info("Renderer: %s%s%s", backend.api_name, backend.spout_capture ? " (Spout output available)" : " - ", backend.spout_capture ? "" : backend.reason.c_str());
	}

	DeviceContext::~DeviceContext()
	{
		// Order matters: GPU objects owned by capture / preview are destroyed while the device is still alive
		logo.reset();
		thumbnails.reset();
		preview.reset();
		ai.reset();                   // holds snapshots too
		ReleaseSelectionSnapshot();
		captures.clear();             // releases their snapshots, so before the snapshots themselves
		tracker->SetWriteObserver({}, nullptr);
		snapshots.reset();
		tracker.reset();
	}

	DeviceContext *DeviceContext::From(device *device)
	{
		return device != nullptr ? device->get_private_data<DeviceContext>() : nullptr;
	}

	DeviceContext *DeviceContext::Create(device *device)
	{
		if (DeviceContext *const existing = From(device))
			return existing;
		DeviceContext *const ctx = DeviceContextAccess::New(device);
		device->set_private_data(reinterpret_cast<const uint8_t *>(&__uuidof(DeviceContext)), reinterpret_cast<uintptr_t>(ctx));
		return ctx;
	}

	void DeviceContext::Destroy(device *device)
	{
		DeviceContext *const ctx = From(device);
		if (ctx == nullptr)
			return;
		device->set_private_data(reinterpret_cast<const uint8_t *>(&__uuidof(DeviceContext)), 0);
		DeviceContextAccess::Delete(ctx);
	}

	// ---------------------------------------------------------------------------------------
	// Capture streams
	// ---------------------------------------------------------------------------------------
	CaptureManager *DeviceContext::CaptureOf(uint32_t resource_id)
	{
		if (resource_id == 0)
			return nullptr;
		for (const std::unique_ptr<CaptureManager> &capture : captures)
			if (capture->IsActive() && capture->SourceId() == resource_id)
				return capture.get();
		return nullptr;
	}

	CaptureManager *DeviceContext::CaptureAt(uint32_t resource_id, uint32_t moment)
	{
		if (resource_id == 0)
			return nullptr;
		for (const std::unique_ptr<CaptureManager> &capture : captures)
			if (capture->IsActive() && capture->SourceId() == resource_id && capture->Moment() == moment)
				return capture.get();
		return nullptr;
	}

	bool DeviceContext::StartCapture(uint32_t resource_id, std::string *error, StreamTransform transform, uint32_t secondary_id,
		uint32_t moment, uint32_t secondary_moment)
	{
		TrackedResource res;
		if (!tracker->Get(resource_id, res))
		{
			if (error != nullptr)
				*error = "Resource #" + std::to_string(resource_id) + " no longer exists";
			return false;
		}
		// The preview may reference the previous output texture: drop it before it gets destroyed
		preview->Clear();
		PrimaryCapture().SetTracker(tracker.get());
		PrimaryCapture().SetSnapshots(snapshots.get());
		PrimaryCapture().SetPresentQueue(present_queue);
		return PrimaryCapture().Start(res, transform, secondary_id, error, moment, secondary_moment);
	}

	bool DeviceContext::StartCaptureInNewSlot(uint32_t resource_id, std::string *error, StreamTransform transform, uint32_t secondary_id,
		uint32_t moment, uint32_t secondary_moment)
	{
		// Re-use the primary slot when it is not capturing anything
		if (!PrimaryCapture().IsActive())
			return StartCapture(resource_id, error, transform, secondary_id, moment, secondary_moment);

		TrackedResource res;
		if (!tracker->Get(resource_id, res))
		{
			if (error != nullptr)
				*error = "Resource #" + std::to_string(resource_id) + " no longer exists";
			return false;
		}
		if (captures.size() >= kSanityStreamLimit)
		{
			// Not a product limit; see kSanityStreamLimit. Reaching it means something is looping.
			if (error != nullptr)
				*error = "Refusing to create more than " + std::to_string(kSanityStreamLimit) + " streams";
			return false;
		}

		// Smallest free stream index (1 = primary)
		unsigned int slot_index = 2;
		for (;; ++slot_index)
		{
			bool taken = false;
			for (const std::unique_ptr<CaptureManager> &capture : captures)
				taken |= capture->SlotIndex() == slot_index;
			if (!taken)
				break;
		}

		auto capture = std::make_unique<CaptureManager>(gpu_device, backend, SenderBaseName(), slot_index);
		capture->SetTracker(tracker.get());
		capture->SetSnapshots(snapshots.get());
		capture->SetPresentQueue(present_queue);
		if (!capture->Start(res, transform, secondary_id, error, moment, secondary_moment))
			return false;
		captures.push_back(std::move(capture));
		log::Info("Additional capture stream %u started", slot_index);
		return true;
	}

	void DeviceContext::StopCapture(size_t slot)
	{
		if (slot >= captures.size())
			return;
		preview->Clear();
		if (slot == 0)
		{
			captures[0]->Stop();
			return;
		}
		captures[slot]->Stop();
		captures.erase(captures.begin() + static_cast<ptrdiff_t>(slot));
	}

	void DeviceContext::StopAllCaptures()
	{
		preview->Clear();
		for (const std::unique_ptr<CaptureManager> &capture : captures)
			capture->Stop();
		captures.resize(1);
	}

	// ---------------------------------------------------------------------------------------
	// Per frame
	// ---------------------------------------------------------------------------------------
	void DeviceContext::OnPresent(command_queue *queue, swapchain *swapchain)
	{
		// 0. Remember the presenting queue: the D3D12 bridge is created on it, possibly from the overlay
		present_queue = queue;
		for (const std::unique_ptr<CaptureManager> &capture : captures)
			capture->SetPresentQueue(queue);

		// 1. Immediate command lists are never "executed": merge whatever they recorded this frame
		for (command_queue *const q : tracker->Queues())
		{
			if (command_list *const immediate = q->get_immediate_command_list())
				if (CommandListState *const state = immediate->get_private_data<CommandListState>())
					tracker->Merge(*state);
		}
		if (command_list *const immediate = queue->get_immediate_command_list())
			if (CommandListState *const state = immediate->get_private_data<CommandListState>())
				tracker->Merge(*state);

		// 2. Close the frame: statistics of this frame become "last frame"
		tracker->EndFrame();
		// Snapshots whose moment did not come this frame are taken now, before anything reads them
		snapshots->EndFrame(queue);

		// 3. Capture copies (GPU) for every active stream, sharing a single optional flush.
		//    One index per presented frame, shared by every stream: this is what a recorder uses to line
		//    the streams of the same frame up (see CyGameCaptureCore/FrameSync.hpp).
		const uint64_t frame_index = ++present_frame_index;

		// Which back buffer holds the frame about to be shown; a stream capturing "the back buffer"
		// follows this rather than one fixed resource of the swap chain.
		const resource current_back_buffer = swapchain->get_current_back_buffer();
		for (const std::unique_ptr<CaptureManager> &capture : captures)
			capture->SetCurrentBackBuffer(current_back_buffer);

		TryDumpBuffers();
		TryAutoCapture();
		TryOpenOverlay();
		TryAiSearchOnStart();
		bool any_copy = false;
		bool needs_flush = false;
		for (const std::unique_ptr<CaptureManager> &capture : captures)
		{
			const bool copying = capture->BeginCopy(queue, *tracker);
			any_copy |= copying;
			needs_flush |= copying && capture->NeedsQueueFlush();   // D3D12: the bridge reads what was just recorded
		}
		if (any_copy && (flush_after_copy || needs_flush))
		{
			LARGE_INTEGER t0, t1, freq;
			QueryPerformanceCounter(&t0);
			queue->flush_immediate_command_list();
			QueryPerformanceCounter(&t1);
			QueryPerformanceFrequency(&freq);
			last_flush_us = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1e6 / static_cast<double>(freq.QuadPart);
		}
		for (const std::unique_ptr<CaptureManager> &capture : captures)
			capture->EndCopy(frame_index);

		// 3b. AI assistant: pictures of the frame for its helper process, when asked. Before the view
		//     mode, which may replace the back buffer the assistant pictures at the end of the frame.
		ai->OnPresent(queue, current_back_buffer);

		// 4. View mode: replace what the player sees (after the captures, so a stream of the back buffer keeps the HUD)
		PollHotkeys();
		ApplyViewMode(queue, swapchain);

		// 5. Preview copy for the inspector selection (only while the overlay is looking at it)
		if (inspector.moment == 0)
			ReleaseSelectionSnapshot();
		const uint64_t frame = tracker->FrameIndex();
		TrackedResource selected;
		if (inspector.selected_id != 0 && tracker->Get(inspector.selected_id, selected))
		{
			if (preview->IsWanted(frame))
			{
				CaptureManager *const capture = CaptureAt(selected.id, 0);
				TrackedResource at_moment;
				if (inspector.moment != 0)
				{
					// Looking at a moment of the frame: show exactly that, whatever a stream does
					if (SelectionAtMoment(selected, at_moment))
						preview->Update(queue, at_moment);
				}
				else if (capture != nullptr && capture->OutputView() != 0 && !preview->IsFrozen())
					preview->UseExternalView(capture->OutputView(), capture->OutputDesc(), selected.id);
				else
					preview->Update(queue, selected);
			}
			else if (view_mode != ViewMode::SelectedBuffer)
				ReleaseSelectionSnapshot();   // neither the preview nor the view mode needs it
		}
		else if (preview->SourceId() != 0 && preview->SourceId() == inspector.selected_id && !preview->IsFrozen())
		{
			preview->Clear();
		}

		// 6. Contact sheet, for the same reason and under the same condition: only while it is on screen
		if (thumbnails_wanted)
		{
			thumbnails->Update(queue, inspector.Entries(), thumbnail_page);
			thumbnails_wanted = false;
			thumbnails_idle_ = 0;
		}
		else if (thumbnails_idle_ < 60 && ++thumbnails_idle_ == 60)
		{
			// A second without the sheet on screen: give the full resolution copies back.
			thumbnails->ReleaseInputs();
		}
	}

	void DeviceContext::TryOpenOverlay()
	{
		if (open_overlay_done_ || runtimes.empty() || tracker->FrameIndex() < 60)
			return;
		open_overlay_done_ = true;
		if (config::GetBool("OpenOverlayOnStart", false))
			runtimes.front()->open_overlay(true, input_source::none);
	}

	void DeviceContext::TryAiSearchOnStart()
	{
		// [CYGAMECAPTURE] AISearchOnStart=<frame>: presses the AI assistant's button once at that frame (60 at
		// the earliest), for automated checks. A game's first seconds are usually black loading screens.
		if (ai_search_on_start_done_)
			return;
		const int at_frame = config::GetInt("AISearchOnStart", 0);
		if (at_frame <= 0)
		{
			ai_search_on_start_done_ = true;
			return;
		}
		if (tracker->FrameIndex() < static_cast<uint64_t>(std::max(at_frame, 60)))
			return;
		ai_search_on_start_done_ = true;
		ai->FindSettingWithoutInterface();
	}

	void DeviceContext::PollHotkeys()
	{
		if (view_toggle_key == 0 || runtimes.empty())
			return;
		// ReShade tracks key state per effect runtime; the first one is the main swap chain
		if (runtimes.front()->is_key_pressed(view_toggle_key))
		{
			view_mode = view_mode == ViewMode::Game ? ViewMode::SelectedBuffer : ViewMode::Game;
			log::Info("View mode: %s", view_mode == ViewMode::Game ? "Game" : "Selected buffer");
		}
	}

	bool DeviceContext::SelectionAtMoment(const TrackedResource &selected, TrackedResource &out)
	{
		if (selection_snapshot_ == 0 || selection_snapshot_source_ != selected.handle || selection_snapshot_moment_ != inspector.moment)
		{
			ReleaseSelectionSnapshot();
			std::string error;
			selection_snapshot_ = snapshots->Acquire(selected, inspector.moment, &error);
			selection_snapshot_source_ = selected.handle;
			selection_snapshot_moment_ = inspector.moment;
			selection_snapshot_error = error;
			if (selection_snapshot_ == 0)
				return false;
		}
		return snapshots->Proxy(selection_snapshot_, selected, out);
	}

	void DeviceContext::ReleaseSelectionSnapshot()
	{
		if (selection_snapshot_ != 0)
			snapshots->Release(selection_snapshot_);
		selection_snapshot_ = 0;
		selection_snapshot_source_ = {};
		selection_snapshot_moment_ = 0;
	}

	void DeviceContext::ApplyViewMode(command_queue *queue, swapchain *swapchain)
	{
		if (view_mode != ViewMode::SelectedBuffer)
		{
			view_message.clear();
			return;
		}

		TrackedResource src;
		if (inspector.selected_id == 0 || !tracker->Get(inspector.selected_id, src))
		{
			view_message = "No buffer selected: the game image is shown unchanged.";
			return;
		}
		// At a moment of the frame, what is shown is that moment: the back buffer right after the
		// tonemapper, for instance, which is the game without the interface drawn over it.
		if (inspector.moment != 0)
		{
			TrackedResource at_moment;
			if (!SelectionAtMoment(src, at_moment))
			{
				view_message = selection_snapshot_error.empty() ? "Waiting for the first snapshot of the selected moment." : selection_snapshot_error;
				return;
			}
			src = at_moment;
		}

		const resource target = swapchain->get_current_back_buffer();
		if (target == 0 || src.handle == target)
			return;
		const resource_desc target_desc = gpu_device->get_resource_desc(target);

		if (src.Width() != target_desc.texture.width || src.Height() != target_desc.texture.height)
		{
			view_message = "Selected buffer (" + std::to_string(src.Width()) + "x" + std::to_string(src.Height()) + ") does not match the back buffer size; scaling blit not implemented yet.";
			return;
		}
		if (src.IsDepthStencil() || IsDepthStencilFormat(src.Format()))
		{
			view_message = "Depth buffers cannot be displayed yet.";
			return;
		}
		if (!AreCopyCompatible(src.Format(), target_desc.texture.format))
		{
			view_message = std::string("Selected buffer format ") + FormatName(src.Format()) + " is not copy compatible with the back buffer (" + FormatName(target_desc.texture.format) + "); conversion pass not implemented yet.";
			return;
		}
		view_message.clear();

		command_list *const cmd_list = queue->get_immediate_command_list();
		const resource_usage src_state = GuessSourceState(src);
		if (src.IsMultisampled())
		{
			cmd_list->barrier(src.handle, src_state, resource_usage::resolve_source);
			cmd_list->barrier(target, resource_usage::present, resource_usage::resolve_dest);
			cmd_list->resolve_texture_region(src.handle, 0, nullptr, target, 0, 0, 0, 0, format_to_default_typed(target_desc.texture.format, 0));
			cmd_list->barrier(target, resource_usage::resolve_dest, resource_usage::present);
			cmd_list->barrier(src.handle, resource_usage::resolve_source, src_state);
		}
		else
		{
			cmd_list->barrier(src.handle, src_state, resource_usage::copy_source);
			cmd_list->barrier(target, resource_usage::present, resource_usage::copy_dest);
			cmd_list->copy_texture_region(src.handle, 0, nullptr, target, 0, nullptr);
			cmd_list->barrier(target, resource_usage::copy_dest, resource_usage::present);
			cmd_list->barrier(src.handle, resource_usage::copy_source, src_state);
		}
	}

	void DeviceContext::TryDumpBuffers()
	{
		if (dump_buffers_done_)
			return;
		const int at_frame = config::GetInt("DumpBuffers", 0);
		if (at_frame <= 0 || tracker->FrameIndex() < static_cast<uint64_t>(at_frame))
			return;
		dump_buffers_done_ = true;

		std::vector<TrackedResource> snapshot = tracker->Snapshot();
		std::sort(snapshot.begin(), snapshot.end(), [](const TrackedResource &a, const TrackedResource &b) { return a.id < b.id; });

		log::Info("---- Buffer dump at frame %llu: %zu tracked resources ----",
			static_cast<unsigned long long>(tracker->FrameIndex()), snapshot.size());
		log::Info("     id  resolution   active       format                 writes draws steady  flags");
		size_t steady_count = 0;

		for (const TrackedResource &res : snapshot)
		{
			if (!res.IsTexture2D() || res.last.writes == 0)
				continue;   // only what was actually written during the last frame is worth choosing from

			const ActiveRect active = res.ActiveArea();
			char active_text[32] = "-";
			if (res.IsDownscaled())
				snprintf(active_text, sizeof(active_text), "%ux%u", active.width, active.height);

			char flags[64] = {};
			snprintf(flags, sizeof(flags), "%s%s%s%s%s",
				res.is_backbuffer ? "backbuffer " : "",
				res.IsDepthStencil() ? "depth " : "",
				res.IsMultisampled() ? "msaa " : "",
				res.last.cleared ? "cleared " : "",
				res.HasMipsOrLayers() ? "mips/layers " : "");

			if (res.IsPersistent(tracker->FrameIndex()))
				steady_count++;

			log::Info("  #%03u  %4ux%-4u  %-11s  %-20s  %5u  %5u  %4.0f%%  %s",
				res.id, res.Width(), res.Height(), active_text, FormatName(res.Format()),
				res.last.writes, res.last.draw_calls, res.WriteRatio(tracker->FrameIndex()) * 100.0f, flags);
		}
		log::Info("---- end of buffer dump: %zu of them written steadily enough to be worth capturing ----", steady_count);

		// The order of the writes matters as much as the list: a game that draws its interface straight
		// into the final image has no clean buffer to pick, only a clean *moment* in that buffer's frame.
		// Consecutive draws into the same target are already folded into one entry by the tracker.
		const std::vector<TimelineEntry> timeline = tracker->TimelineSnapshot();
		log::Info("---- Write order of that frame: %zu entries ----", timeline.size());
		for (const TimelineEntry &entry : timeline)
		{
			if (!entry.is_write)
				continue;
			TrackedResource res;
			if (!tracker->Get(entry.resource_id, res) || !res.IsTexture2D())
				continue;
			log::Info("  event %5u  #%03u  %4ux%-4u  %-20s  %-8s x%u%s", entry.event_index, entry.resource_id,
				res.Width(), res.Height(), FormatName(res.Format()), WriteKindName(entry.kind), entry.count,
				res.is_backbuffer ? "  [back buffer]" : "");
		}
		log::Info("---- end of write order ----");
	}

	void DeviceContext::TryAutoCapture()
	{
		if (auto_capture_done_)
			return;
		if (!auto_capture_parsed_)
		{
			auto_capture_parsed_ = true;
			const std::string spec = config::GetString("AutoCapture", "");
			if (spec.empty())
			{
				auto_capture_done_ = true;
				return;
			}
			// "<width>x<height>:<format>[;<width>x<height>:<format>...]" - first entry = primary stream, others = extra streams
			size_t pos = 0;
			while (pos < spec.size())
			{
				size_t next = spec.find(';', pos);
				if (next == std::string::npos)
					next = spec.size();
				const std::string entry = spec.substr(pos, next - pos);
				pos = next + 1;
				if (entry.empty())
					continue;

				AutoCaptureEntry parsed;
				char format_name[64] = {};
				if (entry[0] == '#')
				{
					// "#<id>[@<write>][:depth|:hud]"
					char tail[64] = {};
					if (sscanf(entry.c_str(), "#%u%63s", &parsed.id, tail) < 1 || parsed.id == 0)
					{
						log::Warning("AutoCapture: could not parse '%s' (expected #<id>[@<write>][:depth|:hud])", entry.c_str());
						continue;
					}
					const char *rest = tail;
					if (rest[0] == '@')
					{
						char *end = nullptr;
						parsed.moment = static_cast<unsigned int>(strtoul(rest + 1, &end, 10));
						rest = end;
					}
					if (rest[0] == ':')
						++rest;
					strncpy(format_name, rest, sizeof(format_name) - 1);
					// the id form carries no format, so the suffix parser below reads the whole field
					memmove(format_name + 1, format_name, sizeof(format_name) - 1);
					format_name[0] = ':';
					format_name[sizeof(format_name) - 1] = 0;
				}
				else if (sscanf(entry.c_str(), "%ux%u:%63s", &parsed.width, &parsed.height, format_name) < 2)
				{
					log::Warning("AutoCapture: could not parse '%s' (expected <width>x<height>:<format> or #<id>)", entry.c_str());
					continue;
				}
				// Optional ":depth" suffix: the entry is a depth buffer to linearise, not a colour copy
				if (char *const suffix = strchr(format_name, ':'))
				{
					*suffix = '\0';
					if (_stricmp(suffix + 1, "depth") == 0)
						parsed.transform = StreamTransform::DepthLinear;
					else if (_stricmp(suffix + 1, "hud") == 0)
						parsed.transform = StreamTransform::HudDifference;
					else if (suffix[1] != 0)
						log::Warning("AutoCapture: unknown suffix ':%s' (':depth' and ':hud' are understood)", suffix + 1);
				}

				if (format_name[0] != '\0')
				{
					char *end = nullptr;
					const unsigned long numeric = strtoul(format_name, &end, 10);
					if (end != nullptr && *end == '\0')
						parsed.format = static_cast<format>(numeric);
					else
						for (uint32_t f = 1; f < 200; ++f)
							if (_stricmp(FormatName(static_cast<format>(f)), format_name) == 0)
							{
								parsed.format = static_cast<format>(f);
								break;
							}
				}
				if (parsed.id != 0 && parsed.moment != 0)
					log::Info("AutoCapture armed: resource #%03u right after write %u of each frame%s", parsed.id, parsed.moment,
						parsed.transform == StreamTransform::HudDifference ? " (HUD isolated against the first stream)" : "");
				else if (parsed.id != 0)
					log::Info("AutoCapture armed: resource #%03u%s", parsed.id,
						parsed.transform == StreamTransform::DepthLinear ? " (linearised depth)" :
						parsed.transform == StreamTransform::HudDifference ? " (HUD isolated against the first stream)" : "");
				else
				log::Info("AutoCapture armed: %ux%u %s%s", parsed.width, parsed.height,
					parsed.format != format::unknown ? FormatName(parsed.format) : "(any format)",
					parsed.transform == StreamTransform::DepthLinear ? " (linearised depth)" :
					parsed.transform == StreamTransform::HudDifference ? " (HUD isolated against the first stream)" : "");
				auto_capture_entries_.push_back(parsed);
			}
			if (auto_capture_entries_.empty())
				auto_capture_done_ = true;
			return;
		}

		// Let the game settle before picking (resources of the first frames are often transient)
		if (tracker->FrameIndex() < 60)
			return;

		std::vector<TrackedResource> snapshot = tracker->Snapshot();
		// by id, so the same configuration selects the same buffer from one run to the next
		std::sort(snapshot.begin(), snapshot.end(), [](const TrackedResource &a, const TrackedResource &b) { return a.id < b.id; });
		bool first = true;
		for (const AutoCaptureEntry &entry : auto_capture_entries_)
		{
			for (const TrackedResource &res : snapshot)
			{
				const bool wants_hud = entry.transform == StreamTransform::HudDifference;
				if (entry.id != 0)
				{
					// An explicit id is an explicit choice: none of the guesswork below applies to it.
					if (res.id != entry.id)
						continue;
				}
				// The finished image is usually the back buffer, so a HUD entry is allowed to pick it
				else if ((res.is_backbuffer && !wants_hud) || res.last.writes == 0 || !res.IsTexture2D() || CaptureOf(res.id) != nullptr)
					continue;
				if (entry.id == 0)
				{
					const bool is_depth = res.IsDepthStencil() || IsDepthStencilFormat(res.Format());
					if (is_depth != (entry.transform == StreamTransform::DepthLinear))
						continue;
					if (res.Width() != entry.width || res.Height() != entry.height)
						continue;
					if (entry.format != format::unknown && format_to_typeless(res.Format()) != format_to_typeless(entry.format))
						continue;
				}
				std::string error;
				// A HUD entry subtracts whatever the first stream is capturing: in a game that is the
				// scene before the interface, which is exactly the buffer the first entry selected.
				const uint32_t secondary_id = wants_hud ? PrimaryCapture().SourceId() : 0;
				const uint32_t secondary_moment = wants_hud ? PrimaryCapture().Moment() : 0;
				const bool ok = first ? StartCapture(res.id, &error, entry.transform, secondary_id, entry.moment, secondary_moment)
									: StartCaptureInNewSlot(res.id, &error, entry.transform, secondary_id, entry.moment, secondary_moment);
				if (ok)
				{
					if (first)
					{
						// The inspector shows what stream 1 captures, at the same moment of the frame
						inspector.selected_id = res.id;
						inspector.moment = entry.moment;
					}
					log::Info("AutoCapture: selected resource #%03u (%s)", res.id, first ? "primary stream" : "extra stream");
				}
				else
					log::Warning("AutoCapture: failed to start on #%03u: %s", res.id, error.c_str());
				break;
			}
			first = false;
		}
		auto_capture_done_ = true;
	}

	// ---------------------------------------------------------------------------------------
	// ReShade events
	// ---------------------------------------------------------------------------------------
	namespace
	{
		void on_init_device(device *device)
		{
			DeviceContext::Create(device);
		}
		void on_destroy_device(device *device)
		{
			// The D3D11On12 bridge holds references to this device and its queue, so it has to go with them:
			// flushing its context after the game's device is gone would dereference freed objects.
			const bool is_d3d12 = device != nullptr && device->get_api() == device_api::d3d12;
			ID3D12Device *const native12 = is_d3d12 ? reinterpret_cast<ID3D12Device *>(device->get_native()) : nullptr;

			DeviceContext::Destroy(device);

			if (native12 != nullptr)
				D3D11On12Bridge::Get().ShutdownForDevice(native12);
		}
		void on_destroy_command_queue(command_queue *queue)
		{
			if (DeviceContext *const ctx = DeviceContext::From(queue->get_device()))
			{
				if (ctx->present_queue == queue)
					ctx->present_queue = nullptr;
				for (const std::unique_ptr<CaptureManager> &capture : ctx->captures)
					capture->ForgetQueue(queue);
			}
		}
		void on_init_swapchain(swapchain *swapchain, bool resize)
		{
			if (DeviceContext *const ctx = DeviceContext::From(swapchain->get_device()))
			{
				ctx->tracker->OnInitSwapchain(swapchain);
				if (resize)
					log::Info("Swap chain resized");
			}
		}
		void on_destroy_swapchain(swapchain *swapchain, bool)
		{
			if (DeviceContext *const ctx = DeviceContext::From(swapchain->get_device()))
				ctx->tracker->OnDestroySwapchain(swapchain);
		}
		void on_present(command_queue *queue, swapchain *swapchain, const rect *, const rect *, uint32_t, const rect *)
		{
			if (DeviceContext *const ctx = DeviceContext::From(swapchain->get_device()))
				ctx->OnPresent(queue, swapchain);
		}
		void on_init_effect_runtime(effect_runtime *runtime)
		{
			if (DeviceContext *const ctx = DeviceContext::From(runtime->get_device()))
				ctx->runtimes.push_back(runtime);
		}
		void on_destroy_effect_runtime(effect_runtime *runtime)
		{
			if (DeviceContext *const ctx = DeviceContext::From(runtime->get_device()))
				ctx->runtimes.erase(std::remove(ctx->runtimes.begin(), ctx->runtimes.end(), runtime), ctx->runtimes.end());
		}
	}

	void RegisterDeviceEvents()
	{
		reshade::register_event<reshade::addon_event::init_device>(on_init_device);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::register_event<reshade::addon_event::destroy_command_queue>(on_destroy_command_queue);
		reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
		reshade::register_event<reshade::addon_event::destroy_swapchain>(on_destroy_swapchain);
		reshade::register_event<reshade::addon_event::present>(on_present);
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
	}

	void UnregisterDeviceEvents()
	{
		reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
		reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::unregister_event<reshade::addon_event::destroy_command_queue>(on_destroy_command_queue);
		reshade::unregister_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
		reshade::unregister_event<reshade::addon_event::destroy_swapchain>(on_destroy_swapchain);
		reshade::unregister_event<reshade::addon_event::present>(on_present);
		reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
	}
}
