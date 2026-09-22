// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "CaptureManager.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "Backends/D3D12/D3D11On12Bridge.hpp"
#include "Addon/Log.hpp"
#include "Addon/Config.hpp"
#include <CyGameCaptureCore/StreamInfo.hpp>

#include <Windows.h>

#include <cmath>
#include <d3d11.h>
#include <d3d12.h>

using namespace reshade::api;

namespace
{
	/**
	 * Locks and records the owning thread, so a callback that re-enters the same object on the same
	 * thread can tell and back out instead of dead-locking. See CaptureManager::lock_owner_.
	 */
	struct OwnedLock
	{
		OwnedLock(std::mutex &mutex, std::atomic<unsigned long> &owner) : lock(mutex), owner(owner)
		{
			owner.store(GetCurrentThreadId(), std::memory_order_release);
		}
		~OwnedLock() { owner.store(0, std::memory_order_release); }

		std::lock_guard<std::mutex> lock;
		std::atomic<unsigned long> &owner;
	};
}

namespace cygc
{
	namespace
	{
		uint64_t NowTicks()
		{
			LARGE_INTEGER counter;
			QueryPerformanceCounter(&counter);
			return static_cast<uint64_t>(counter.QuadPart);
		}
		uint64_t TicksPerSecond()
		{
			static const uint64_t frequency = [] {
				LARGE_INTEGER f;
				QueryPerformanceFrequency(&f);
				return static_cast<uint64_t>(f.QuadPart);
			}();
			return frequency;
		}

	}

	const char *CaptureStateName(CaptureState state)
	{
		switch (state)
		{
		case CaptureState::Idle: return "Idle";
		case CaptureState::Active: return "Active";
		case CaptureState::SourceLost: return "Capture Source Lost";
		case CaptureState::Unsupported: return "Unsupported";
		case CaptureState::Error: return "Error";
		}
		return "?";
	}

	CaptureManager::CaptureManager(device *device, const BackendSupport &backend, std::string sender_base_name, unsigned int slot_index) :
		device_(device), backend_(backend), sender_base_name_(std::move(sender_base_name)), slot_index_(slot_index < 1 ? 1 : slot_index),
		uses_bridge_(backend.api == device_api::d3d12)
	{
	}

	CaptureManager::~CaptureManager()
	{
		Stop();
	}

	void CaptureManager::SetCurrentBackBuffer(resource back_buffer)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		current_back_buffer_ = back_buffer;
	}

	bool CaptureManager::ResolveSource(const ResourceTracker &tracker, const TrackedResource &selected, TrackedResource &out) const
	{
		if (!selected.is_backbuffer || current_back_buffer_ == 0 || current_back_buffer_ == selected.handle)
		{
			out = selected;
			return true;
		}
		// Every back buffer of a swap chain shares one description, so nothing else has to change
		if (!tracker.GetByHandle(current_back_buffer_, out))
		{
			out = selected;
			return true;
		}
		return true;
	}

	void CaptureManager::ForgetQueue(command_queue *queue)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (present_queue_ != queue)
			return;
		present_queue_ = nullptr;
		bridge_queue_native_ = nullptr;
	}

	void CaptureManager::SetPresentQueue(command_queue *queue)
	{
		if (queue == nullptr)
			return;
		std::lock_guard<std::mutex> lock(mutex_);
		present_queue_ = queue;
		bridge_queue_native_ = reinterpret_cast<void *>(queue->get_native());
	}

	void CaptureManager::SetState(CaptureState state, std::string message)
	{
		state_ = state;
		message_ = std::move(message);
	}

	StreamTransform CaptureManager::Transform() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return transform_;
	}

	DepthSettings CaptureManager::GetDepthSettings() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return depth_settings_;
	}

	void CaptureManager::SetDepthSettings(const DepthSettings &settings)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		depth_settings_ = settings;   // picked up by the next frame, nothing to rebuild
	}

	uint32_t CaptureManager::SecondarySourceId() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return secondary_id_;
	}

	uint32_t CaptureManager::Moment() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return moment_;
	}

	uint32_t CaptureManager::SecondaryMoment() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return secondary_moment_;
	}

	void CaptureManager::ReleaseSnapshots()
	{
		if (snapshots_ != nullptr)
		{
			snapshots_->Release(snapshot_ticket_);
			snapshots_->Release(secondary_snapshot_ticket_);
		}
		snapshot_ticket_ = 0;
		secondary_snapshot_ticket_ = 0;
		moment_ = 0;
		secondary_moment_ = 0;
	}

	HudSettings CaptureManager::GetHudSettings() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return hud_settings_;
	}

	void CaptureManager::SetHudSettings(const HudSettings &settings)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		hud_settings_ = settings;
	}

	bool CaptureManager::Start(const TrackedResource &source, StreamTransform transform, uint32_t secondary_id, std::string *error,
		uint32_t moment, uint32_t secondary_moment)
	{
		const OwnedLock lock(mutex_, lock_owner_);

		auto fail = [&](CaptureState state, const std::string &message) {
			SetState(state, message);
			log::Warning("%s", message.c_str());
			if (error != nullptr)
				*error = message;
			return false;
		};

		if (!backend_.spout_capture)
			return fail(CaptureState::Unsupported, "Spout output not available on " + std::string(backend_.api_name) + ": " + backend_.reason);
		if (!source.IsTexture2D() || source.Width() == 0 || source.Height() == 0)
			return fail(CaptureState::Unsupported, "Selected resource is not a 2D texture");
		const bool is_depth = source.IsDepthStencil() || IsDepthStencilFormat(source.Format());
		if (transform == StreamTransform::DepthLinear)
		{
			if (!is_depth)
				return fail(CaptureState::Unsupported, "The depth stream needs a depth resource; use it as a plain capture source instead");
			if (source.IsMultisampled())
				return fail(CaptureState::Unsupported, "Multisampled depth cannot be linearised yet (it would need a resolve pass first)");

			// The numbers below cannot be detected from outside the game, so they come from the config
			// (and can be changed live in the Capture tab). Same meaning as ReShade's RESHADE_DEPTH_*.
			depth_settings_.far_plane = config::GetFloat("DepthFarPlane", 1000.0f);
			depth_settings_.reversed = config::GetBool("DepthReversed", false);
			depth_settings_.logarithmic = config::GetBool("DepthLogarithmic", false);
			depth_settings_.upside_down = config::GetBool("DepthUpsideDown", false);

			std::string pass_error;
			if (!depth_pass_.IsValid() && !CreateDepthPass(device_, depth_pass_, pass_error))
				return fail(CaptureState::Unsupported, "Depth linearisation pass unavailable: " + pass_error);
		}
		else if (transform == StreamTransform::HudDifference)
		{
			if (is_depth)
				return fail(CaptureState::Unsupported, "The HUD stream compares two colour buffers, not depth");
			// One buffer at two different moments is a valid pair: that is how a game that draws its
			// interface straight into the final image gets a HUD stream at all.
			if (secondary_id == 0 || (secondary_id == source.id && secondary_moment == moment))
				return fail(CaptureState::Unsupported, "The HUD stream needs a second buffer, or the same buffer at an earlier moment of the frame: the scene before the interface was drawn");
			if (!IsSpoutCompatibleFormat(source.Format()))
				return fail(CaptureState::Unsupported, std::string("Unsupported format ") + FormatName(source.Format()));

			hud_settings_.threshold = config::GetFloat("HudThreshold", 0.02f);
			hud_settings_.softness = config::GetFloat("HudSoftness", 0.08f);
			hud_settings_.keep_final_colour = config::GetBool("HudKeepFinalColour", true);

			std::string pass_error;
			if (!hud_pass_.IsValid() && !CreateHudPass(device_, hud_pass_, pass_error))
				return fail(CaptureState::Unsupported, "HUD isolation pass unavailable: " + pass_error);
			secondary_id_ = secondary_id;
		}
		else
		{
			if (is_depth)
				return fail(CaptureState::Unsupported, "Depth resources need the depth stream (\"Use As Depth Stream\"), not a plain copy");
			if (source.IsMultisampled() && !IsSpoutCompatibleFormat(source.Format()))
				return fail(CaptureState::Unsupported, std::string("Unsupported format ") + FormatName(source.Format()) +
					" on a multisampled resource: it would need a resolve before the conversion");
		}
		transform_ = transform;

		if (uses_bridge_ && bridge_queue_native_ == nullptr)
			return fail(CaptureState::Error, "The presenting command queue is not known yet (D3D12 bridge); try again once the game rendered a frame");

		std::string local_error;
		if (!EnsureOutput(source.desc, &local_error))
			return fail(CaptureState::Error, local_error);

		// Moments of the frame: the snapshots are asked for here, and read in BeginCopy
		ReleaseSnapshots();
		if (moment != 0 || (transform == StreamTransform::HudDifference && secondary_moment != 0))
		{
			if (snapshots_ == nullptr)
				return fail(CaptureState::Unsupported, "Capturing at a moment of the frame is not available here");
			if (moment != 0)
			{
				snapshot_ticket_ = snapshots_->Acquire(source, moment, &local_error);
				if (snapshot_ticket_ == 0)
					return fail(CaptureState::Unsupported, local_error);
			}
			if (transform == StreamTransform::HudDifference && secondary_moment != 0)
			{
				TrackedResource clean;
				if (tracker_ == nullptr || !tracker_->Get(secondary_id, clean))
				{
					ReleaseSnapshots();
					return fail(CaptureState::SourceLost, "The scene buffer #" + std::to_string(secondary_id) + " no longer exists");
				}
				secondary_snapshot_ticket_ = snapshots_->Acquire(clean, secondary_moment, &local_error);
				if (secondary_snapshot_ticket_ == 0)
				{
					ReleaseSnapshots();
					return fail(CaptureState::Unsupported, local_error);
				}
			}
			moment_ = moment;
			secondary_moment_ = transform == StreamTransform::HudDifference ? secondary_moment : 0;
		}

		source_ = source.handle;
		source_id_ = source.id;
		source_desc_ = source.desc;
		frames_sent_ = 0;
		frames_skipped_ = 0;
		fps_ = 0.0f;
		fps_window_start_ = NowTicks();
		fps_window_frames_ = 0;
		SetState(CaptureState::Active, "");

		log::Info("Selected capture source #%03u %ux%u %s -> sender '%s' (%s%s)", source.id, source.Width(), source.Height(),
			FormatName(source.Format()), sender_.Name().c_str(), FormatName(output_desc_.texture.format),
			transform_ == StreamTransform::DepthLinear ? ", linearised depth" :
			transform_ == StreamTransform::HudDifference ? ", HUD isolated" : "");
		if (moment_ != 0)
			log::Info("  read right after write %u of each frame, not at its end", moment_);
		if (secondary_moment_ != 0)
			log::Info("  scene: #%03u right after write %u of each frame", secondary_id_, secondary_moment_);
		return true;
	}

	void CaptureManager::Stop()
	{
		const OwnedLock lock(mutex_, lock_owner_);
		if (state_ == CaptureState::Idle && output_ == 0 && !sender_.IsCreated())
			return;
		DestroyOutput();
		DestroyPassInputs();
		ReleaseSnapshots();
		frame_sync_.Release();
		sender_.Release();
		transform_ = StreamTransform::Copy;
		secondary_id_ = 0;
		source_ = {};
		source_id_ = 0;
		source_desc_ = {};
		SetState(CaptureState::Idle, "");
		log::Info("Capture stopped");
	}

	void CaptureManager::ForgetOurResource(resource resource)
	{
		// Without this the tracker would list the add-on's own textures in the Buffer Inspector, and - far
		// worse - would report their destruction back into this object while it holds its own lock.
		if (tracker_ != nullptr)
			tracker_->Forget(resource);
	}

	format CaptureManager::OutputFormatFor(format source_format) const
	{
		// A linearised depth stream is always published as 16 bits per channel: that is the precision a
		// depth-based reprojection needs, and it is a format every Spout receiver can open.
		switch (transform_)
		{
		case StreamTransform::DepthLinear:
			return kDepthStreamFormat;
		case StreamTransform::HudDifference:
			return kHudStreamFormat;   // the interface needs an alpha channel to be laid over anything
		default:
			return ToPublishableFormat(source_format);
		}
	}

	bool CaptureManager::PrepareInput(PassInput &input, const TrackedResource &source, command_list *cmd_list)
	{
		// The shared helper does the work; what is specific here is that the readable copy it may
		// create belongs to the add-on and must never end up in the tracker.
		return cygc::PrepareInput(device_, input, source, cmd_list,
			[this](resource res) { ForgetOurResource(res); });
	}

	void CaptureManager::DestroyPassInputs()
	{
		cygc::DestroyInput(device_, primary_input_, [this](resource res) { ForgetOurResource(res); });
		cygc::DestroyInput(device_, secondary_input_, [this](resource res) { ForgetOurResource(res); });
	}

	bool CaptureManager::EnsureOutput(const resource_desc &source_desc, std::string *error)
	{
		const format out_format = OutputFormatFor(source_desc.texture.format);
		const uint32_t width = source_desc.texture.width;
		const uint32_t height = source_desc.texture.height;

		if (output_ != 0 && output_desc_.texture.width == width && output_desc_.texture.height == height && output_desc_.texture.format == out_format)
			return true;

		const bool recreate = output_ != 0;
		DestroyOutput();

		const bool created = uses_bridge_
			? CreateSharedOutputD3D12(width, height, out_format, error)
			: CreateSharedOutputD3D11(width, height, out_format, error);
		if (!created)
		{
			DestroyOutput();
			return false;
		}

		if (!device_->create_resource_view(output_, resource_usage::shader_resource, resource_view_desc(output_desc_.texture.format), &output_srv_))
		{
			log::Warning("Failed to create shader resource view on the capture texture (preview of the output disabled)");
			output_srv_ = {};
		}

		// Anything that renders into the output instead of copying into it needs a render target view.
		// The plain copy may need one too: a dynamically scaled source is blitted, not copied.
		if (true &&
			!device_->create_resource_view(output_, resource_usage::render_target, resource_view_desc(output_desc_.texture.format), &output_rtv_))
		{
			if (error != nullptr)
				*error = "Failed to create a render target view on the capture texture";
			log::Error("Failed to create a render target view on the capture texture");
			DestroyOutput();
			return false;
		}

		const uint32_t dxgi_format = ToDxgiFormat(output_desc_.texture.format);
		bool sender_ok;
		if (sender_.IsCreated())
			sender_ok = sender_.Update(width, height, share_handle_, dxgi_format);
		else
			sender_ok = sender_.Create(SpoutSender::MakeUniqueName(sender_base_name_, slot_index_), width, height, share_handle_, dxgi_format);

		if (!sender_ok)
		{
			if (error != nullptr)
			{
				std::string reason;
				*error = SpoutSender::CanRegisterNewSender(&reason) ? "Failed to register the Spout sender" : reason;
			}
			DestroyOutput();
			return false;
		}

		// The recorder matches the sync slot to the Spout sender by name, so claim it with the same name
		if (!frame_sync_.IsClaimed())
		{
			const uint64_t group_id = (static_cast<uint64_t>(GetCurrentProcessId()) << 32) |
				static_cast<uint32_t>(reinterpret_cast<uintptr_t>(device_) & 0xFFFFFFFFu);
			if (!frame_sync_.Claim(sender_.Name(), group_id))
				log::Warning("Frame sync table full: '%s' will be recorded without cross-stream alignment", sender_.Name().c_str());
		}

		if (recreate)
			log::Info("Capture texture recreated: %ux%u %s%s", width, height, FormatName(output_desc_.texture.format), uses_bridge_ ? " (D3D11On12 bridge)" : "");
		else
			log::Info("Capture texture created: %ux%u %s (shared handle 0x%p)%s", width, height, FormatName(output_desc_.texture.format), share_handle_, uses_bridge_ ? " via D3D11On12 bridge" : "");
		return true;
	}

	// D3D10 / D3D11: ReShade creates the texture with D3D11_RESOURCE_MISC_SHARED, the legacy DXGI handle
	// it returns is exactly what Spout receivers open. One GPU copy per frame, no intermediate.
	bool CaptureManager::CreateSharedOutputD3D11(uint32_t width, uint32_t height, format out_format, std::string *error)
	{
		const resource_desc desc(width, height, 1, 1, out_format, 1, memory_heap::default_,
			resource_usage::render_target | resource_usage::shader_resource | resource_usage::copy_dest | resource_usage::resolve_dest,
			resource_flags::shared);

		share_handle_ = nullptr;
		if (!device_->create_resource(desc, nullptr, resource_usage::copy_dest, &output_, &share_handle_) || share_handle_ == nullptr)
		{
			if (error != nullptr)
				*error = std::string("Failed to create shared capture texture ") + std::to_string(width) + "x" + std::to_string(height) + " " + FormatName(out_format);
			log::Error("%s", error != nullptr ? error->c_str() : "Failed to create shared capture texture");
			return false;
		}
		ForgetOurResource(output_);
		output_desc_ = device_->get_resource_desc(output_);
		return true;
	}

	// D3D12: a plain intermediate on the game device (the game queue copies into it), wrapped for the
	// D3D11On12 bridge, plus the real shared texture created on the bridge's D3D11 device.
	bool CaptureManager::CreateSharedOutputD3D12(uint32_t width, uint32_t height, format out_format, std::string *error)
	{
		D3D11On12Bridge &bridge = D3D11On12Bridge::Get();
		std::string bridge_error;
		log::Verbose("D3D12 capture output: %ux%u %s (bridge ready: %s)", width, height, FormatName(out_format), bridge.IsReady() ? "yes" : "no");
		if (!bridge.IsReady())
		{
			ID3D12Device *const device12 = reinterpret_cast<ID3D12Device *>(device_->get_native());
			ID3D12CommandQueue *const queue12 = reinterpret_cast<ID3D12CommandQueue *>(bridge_queue_native_);
			if (!bridge.Initialize(device12, queue12, bridge_error))
			{
				if (error != nullptr)
					*error = "D3D11On12 bridge: " + bridge_error;
				log::Error("D3D11On12 bridge: %s", bridge_error.c_str());
				return false;
			}
		}

		// Intermediate: written by the game queue as a copy destination, read by the bridge
		const resource_desc desc(width, height, 1, 1, out_format, 1, memory_heap::default_,
			resource_usage::render_target | resource_usage::shader_resource | resource_usage::copy_dest | resource_usage::copy_source | resource_usage::resolve_dest);
		if (!device_->create_resource(desc, nullptr, resource_usage::copy_dest, &output_))
		{
			if (error != nullptr)
				*error = std::string("Failed to create the D3D12 intermediate texture ") + std::to_string(width) + "x" + std::to_string(height) + " " + FormatName(out_format);
			log::Error("%s", error != nullptr ? error->c_str() : "Failed to create the D3D12 intermediate texture");
			return false;
		}
		ForgetOurResource(output_);
		output_desc_ = device_->get_resource_desc(output_);

		std::lock_guard<std::mutex> bridge_lock(bridge.Lock());

		// In D3D12 a reshade::api::resource handle *is* the ID3D12Resource pointer
		ID3D12Resource *const native = reinterpret_cast<ID3D12Resource *>(output_.handle);
		log::Verbose("D3D12 capture output: wrapping intermediate 0x%p...", native);
		if (!bridge.WrapResource(native, D3D12_RESOURCE_STATE_COPY_DEST, &bridge_wrapped_, bridge_error))
		{
			if (error != nullptr)
				*error = "D3D11On12 bridge: " + bridge_error;
			log::Error("D3D11On12 bridge: %s", bridge_error.c_str());
			return false;
		}
		if (!bridge.CreateSharedTexture(width, height, ToDxgiFormat(output_desc_.texture.format), &bridge_shared_, &share_handle_, bridge_error))
		{
			if (error != nullptr)
				*error = "D3D11On12 bridge: " + bridge_error;
			log::Error("D3D11On12 bridge: %s", bridge_error.c_str());
			return false;
		}
		log::Verbose("D3D12 capture output: wrapped 0x%p, shared texture 0x%p handle 0x%p", bridge_wrapped_, bridge_shared_, share_handle_);
		return true;
	}

	void CaptureManager::DestroyOutput()
	{
		if (bridge_wrapped_ != nullptr || bridge_shared_ != nullptr)
		{
			// The GPU may still be executing copies that reference the wrapped intermediate and the shared
			// texture. Releasing them now would be a use-after-free on the GPU (device removed / driver
			// reset), so drain the queue first. Only happens on stop / resize, never per frame.
			if (present_queue_ != nullptr)
				present_queue_->wait_idle();

			D3D11On12Bridge &bridge = D3D11On12Bridge::Get();
			std::lock_guard<std::mutex> bridge_lock(bridge.Lock());
			if (bridge_wrapped_ != nullptr)
			{
				bridge_wrapped_->Release();
				bridge_wrapped_ = nullptr;
			}
			if (bridge_shared_ != nullptr)
			{
				bridge_shared_->Release();
				bridge_shared_ = nullptr;
			}
		}
		if (output_srv_ != 0)
			device_->destroy_resource_view(output_srv_);
		if (output_rtv_ != 0)
			device_->destroy_resource_view(output_rtv_);
		if (output_ != 0)
			device_->destroy_resource(output_);
		output_rtv_ = {};
		output_srv_ = {};
		output_ = {};
		output_desc_ = {};
		share_handle_ = nullptr;
	}

	void CaptureManager::OnResourceDestroyed(resource resource, uint32_t id)
	{
		// Re-entering from our own destruction of our own texture: nothing to do, and taking the lock
		// again on this thread would throw std::system_error and bring the game down.
		if (lock_owner_.load(std::memory_order_acquire) == GetCurrentThreadId())
			return;

		std::lock_guard<std::mutex> lock(mutex_);
		if (state_ != CaptureState::Active || resource != source_)
			return;
		source_ = {};
		SetState(CaptureState::SourceLost, "Capture Source Lost: resource #" + std::to_string(id) + " was destroyed by the game. Select a new buffer.");
		log::Warning("Capture source lost (#%03u destroyed)", id);
	}

	bool CaptureManager::BeginCopy(command_queue *queue, const ResourceTracker &tracker)
	{
		const OwnedLock lock(mutex_, lock_owner_);
		copy_in_progress_ = false;
		if (state_ != CaptureState::Active || source_ == 0)
			return false;

		present_queue_ = queue;                                                 // refreshed every frame
		bridge_queue_native_ = reinterpret_cast<void *>(queue->get_native());

		TrackedResource selected;
		if (!tracker.GetByHandle(source_, selected))
		{
			source_ = {};
			SetState(CaptureState::SourceLost, "Capture Source Lost: resource is no longer tracked. Select a new buffer.");
			log::Warning("Capture source lost (#%03u no longer tracked)", source_id_);
			return false;
		}

		// A back buffer selection follows the swap chain rather than one fixed resource
		TrackedResource source;
		ResolveSource(tracker, selected, source);

		// At a moment of the frame, the copy taken at that moment stands in for the buffer
		if (moment_ != 0)
		{
			TrackedResource at_moment;
			if (snapshots_ == nullptr || !snapshots_->Proxy(snapshot_ticket_, source, at_moment))
			{
				frames_skipped_++;   // the first snapshot comes with the next frame
				return false;
			}
			source = at_moment;
		}

		// Dynamic resolution: the game renders into part of a fixed-size target. Said once on entering
		// and once on leaving -- the scale itself changes every frame, and belongs in the status, not in
		// the log.
		const ActiveRect active = source.ActiveArea();
		active_rect_ = active;
		if (source.IsDownscaled() != downscaled_)
		{
			downscaled_ = source.IsDownscaled();
			if (downscaled_)
				log::Info("#%03u uses dynamic resolution: %ux%u rendered inside a %ux%u target. The active "
					"area is scaled into the stream, so the stream keeps its size.",
					source.id, active.width, active.height, source.Width(), source.Height());
			else
				log::Info("#%03u is back to its full %ux%u", source.id, source.Width(), source.Height());
		}
		const SourceRect source_rect = MakeSourceRect(active, source.Width(), source.Height());

		if (primary_input_.viewed != 0 && primary_input_.copy == 0 && primary_input_.viewed != source.handle)
			cygc::DestroyInput(device_, primary_input_, [this](resource res) { ForgetOurResource(res); });   // the resource behind the handle changed; the view is meaningless

		if (source.desc.texture.width != source_desc_.texture.width || source.desc.texture.height != source_desc_.texture.height || source.desc.texture.format != source_desc_.texture.format)
		{
			log::Info("Resolution changed: #%03u %ux%u %s -> %ux%u %s", source.id,
				source_desc_.texture.width, source_desc_.texture.height, FormatName(source_desc_.texture.format),
				source.Width(), source.Height(), FormatName(source.Format()));
			source_desc_ = source.desc;
			std::string error;
			if (transform_ == StreamTransform::Copy && source.IsMultisampled() && !IsSpoutCompatibleFormat(source.Format()))
			{
				SetState(CaptureState::Unsupported, std::string("Unsupported format ") + FormatName(source.Format()) + " on a multisampled resource");
				return false;
			}
			if (!EnsureOutput(source.desc, &error))
			{
				SetState(CaptureState::Error, error);
				return false;
			}
		}

		copy_start_ticks_ = NowTicks();
		if (!sender_.BeginFrame())
		{
			frames_skipped_++;
			return false;
		}
		last_mutex_wait_us_ = static_cast<double>(NowTicks() - copy_start_ticks_) * 1e6 / static_cast<double>(TicksPerSecond());

		command_list *const cmd_list = queue->get_immediate_command_list();
		const resource_usage source_state = GuessSourceState(source);

		if (transform_ == StreamTransform::DepthLinear)
		{
			// PrepareInput records the copy first when the buffer cannot be sampled directly
			if (!PrepareInput(primary_input_, source, cmd_list))
			{
				sender_.EndFrame();
				SetState(CaptureState::Error, "Could not make the depth buffer readable by the linearisation pass");
				return false;
			}

			const bool direct = primary_input_.copy == 0;
			if (direct)
				cmd_list->barrier(source.handle, source_state, resource_usage::shader_resource);
			cmd_list->barrier(output_, resource_usage::copy_dest, resource_usage::render_target);

			DepthConstants constants = MakeDepthConstants(depth_settings_);
			constants.source_rect[0] = source_rect.scale_x;
			constants.source_rect[1] = source_rect.scale_y;
			constants.source_rect[2] = source_rect.offset_x;
			constants.source_rect[3] = source_rect.offset_y;
			depth_pass_.Draw(cmd_list, output_rtv_, &primary_input_.view, 1, &constants,
				sizeof(constants) / sizeof(uint32_t), output_desc_.texture.width, output_desc_.texture.height);

			cmd_list->barrier(output_, resource_usage::render_target, resource_usage::copy_dest);
			if (direct)
				cmd_list->barrier(source.handle, resource_usage::shader_resource, source_state);
		}
		else if (transform_ == StreamTransform::HudDifference)
		{
			TrackedResource selected_clean;
			if (!tracker.Get(secondary_id_, selected_clean))
			{
				sender_.EndFrame();
				SetState(CaptureState::SourceLost, "Capture Source Lost: the scene buffer #" + std::to_string(secondary_id_) +
					" the HUD stream subtracts is gone. Select the two buffers again.");
				return false;
			}
			TrackedResource clean;
			ResolveSource(tracker, selected_clean, clean);
			if (secondary_moment_ != 0)
			{
				TrackedResource at_moment;
				if (snapshots_ == nullptr || !snapshots_->Proxy(secondary_snapshot_ticket_, clean, at_moment))
				{
					sender_.EndFrame();
					frames_skipped_++;
					return false;
				}
				clean = at_moment;
			}

			// The two buffers do not have to be the same size any more: with dynamic resolution the
			// scene is routinely smaller than the finished image. What they must share is an aspect
			// ratio, otherwise the comparison would be between different parts of the picture.
			const ActiveRect clean_active = clean.ActiveArea();
			const double source_aspect = static_cast<double>(active.width) / active.height;
			const double clean_aspect = static_cast<double>(clean_active.width) / clean_active.height;
			if (clean_active.width == 0 || active.width == 0 || std::abs(source_aspect - clean_aspect) > 0.01)
			{
				sender_.EndFrame();
				SetState(CaptureState::Unsupported, "The two buffers of the HUD stream must have the same aspect ratio");
				return false;
			}
			if (!PrepareInput(primary_input_, source, cmd_list) || !PrepareInput(secondary_input_, clean, cmd_list))
			{
				sender_.EndFrame();
				SetState(CaptureState::Error, "Could not make the two buffers readable by the HUD pass");
				return false;
			}

			const resource_usage clean_state = GuessSourceState(clean);
			const bool direct_final = primary_input_.copy == 0;
			const bool direct_clean = secondary_input_.copy == 0;
			if (direct_final)
				cmd_list->barrier(source.handle, source_state, resource_usage::shader_resource);
			if (direct_clean)
				cmd_list->barrier(clean.handle, clean_state, resource_usage::shader_resource);
			cmd_list->barrier(output_, resource_usage::copy_dest, resource_usage::render_target);

			const resource_view inputs[2] = { primary_input_.view, secondary_input_.view };
			const SourceRect clean_rect = MakeSourceRect(clean_active, clean.Width(), clean.Height());
			HudConstants constants = MakeHudConstants(hud_settings_);
			constants.final_rect[0] = source_rect.scale_x;
			constants.final_rect[1] = source_rect.scale_y;
			constants.final_rect[2] = source_rect.offset_x;
			constants.final_rect[3] = source_rect.offset_y;
			constants.clean_rect[0] = clean_rect.scale_x;
			constants.clean_rect[1] = clean_rect.scale_y;
			constants.clean_rect[2] = clean_rect.offset_x;
			constants.clean_rect[3] = clean_rect.offset_y;
			hud_pass_.Draw(cmd_list, output_rtv_, inputs, 2, &constants,
				sizeof(constants) / sizeof(uint32_t), output_desc_.texture.width, output_desc_.texture.height);

			cmd_list->barrier(output_, resource_usage::render_target, resource_usage::copy_dest);
			if (direct_clean)
				cmd_list->barrier(clean.handle, resource_usage::shader_resource, clean_state);
			if (direct_final)
				cmd_list->barrier(source.handle, resource_usage::shader_resource, source_state);
		}
		else if (!source.IsMultisampled() &&
			(!source_rect.IsIdentity() || ToOutputFormat(source.Format()) != output_desc_.texture.format))
		{
			// The same pass covers both cases a plain copy cannot: a source only partly rendered into
			// (dynamic resolution), and a source whose format no receiver would open.
			std::string pass_error;
			if (blit_pass_.IsValid() && blit_pass_.RenderTargetFormat() != output_desc_.texture.format)
				blit_pass_.Destroy();
			if (!blit_pass_.IsValid() && !CreateBlitPass(device_, output_desc_.texture.format, blit_pass_, pass_error))
			{
				sender_.EndFrame();
				SetState(CaptureState::Error, "Scaling pass unavailable: " + pass_error);
				return false;
			}
			if (!PrepareInput(primary_input_, source, cmd_list))
			{
				sender_.EndFrame();
				SetState(CaptureState::Error, "Could not make the buffer readable by the scaling pass");
				return false;
			}

			const bool direct = primary_input_.copy == 0;
			if (direct)
				cmd_list->barrier(source.handle, source_state, resource_usage::shader_resource);
			cmd_list->barrier(output_, resource_usage::copy_dest, resource_usage::render_target);

			const BlitConstants constants { { source_rect.scale_x, source_rect.scale_y, source_rect.offset_x, source_rect.offset_y } };
			blit_pass_.Draw(cmd_list, output_rtv_, &primary_input_.view, 1, &constants,
				sizeof(constants) / sizeof(uint32_t), output_desc_.texture.width, output_desc_.texture.height);

			cmd_list->barrier(output_, resource_usage::render_target, resource_usage::copy_dest);
			if (direct)
				cmd_list->barrier(source.handle, resource_usage::shader_resource, source_state);
		}
		else if (source.IsMultisampled())
		{
			cmd_list->barrier(source.handle, source_state, resource_usage::resolve_source);
			cmd_list->barrier(output_, resource_usage::copy_dest, resource_usage::resolve_dest);
			cmd_list->resolve_texture_region(source.handle, 0, nullptr, output_, 0, 0, 0, 0, output_desc_.texture.format);
			cmd_list->barrier(output_, resource_usage::resolve_dest, resource_usage::copy_dest);
			cmd_list->barrier(source.handle, resource_usage::resolve_source, source_state);
		}
		else
		{
			cmd_list->barrier(source.handle, source_state, resource_usage::copy_source);
			cmd_list->copy_texture_region(source.handle, 0, nullptr, output_, 0, nullptr);
			cmd_list->barrier(source.handle, resource_usage::copy_source, source_state);
		}

		copy_in_progress_ = true;
		return true;
	}

	void CaptureManager::EndCopy(uint64_t frame_index)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!copy_in_progress_)
			return;
		copy_in_progress_ = false;

		if (uses_bridge_ && bridge_wrapped_ != nullptr && bridge_shared_ != nullptr)
		{
			// The caller flushed the immediate command list, so the game copy into the intermediate is
			// already submitted to the queue. The bridge submits to that same queue, so the GPU keeps order.
			const uint64_t bridge_start = NowTicks();
			D3D11On12Bridge &bridge = D3D11On12Bridge::Get();
			{
				std::lock_guard<std::mutex> bridge_lock(bridge.Lock());
				bridge.CopyWrappedToShared(bridge_wrapped_, bridge_shared_);
			}
			last_bridge_us_ = static_cast<double>(NowTicks() - bridge_start) * 1e6 / static_cast<double>(TicksPerSecond());
		}

		sender_.EndFrame();

		// Published after the copy is submitted and the Spout mutex released: from here on, a recorder
		// reading this stream gets the picture of `frame_index`. Every stream of the same present shares
		// that index, which is what lets a recorder align them.
		frame_sync_.Publish(frame_index, output_desc_.texture.width, output_desc_.texture.height,
			ToDxgiFormat(output_desc_.texture.format),
			static_cast<uint32_t>(transform_ == StreamTransform::DepthLinear ? StreamKind::Depth :
				transform_ == StreamTransform::HudDifference ? StreamKind::UI : StreamKind::Color));

		frames_sent_++;
		fps_window_frames_++;
		const uint64_t now = NowTicks();
		last_copy_cpu_us_ = static_cast<double>(now - copy_start_ticks_) * 1e6 / static_cast<double>(TicksPerSecond());
		if (now - fps_window_start_ >= TicksPerSecond() / 2)
		{
			fps_ = static_cast<float>(fps_window_frames_ * static_cast<double>(TicksPerSecond()) / static_cast<double>(now - fps_window_start_));
			fps_window_start_ = now;
			fps_window_frames_ = 0;
		}
	}

	CaptureStatus CaptureManager::GetStatus() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		CaptureStatus status;
		status.state = state_;
		status.message = message_;
		status.sender_name = sender_.Name();
		status.source_id = source_id_;
		status.active_width = active_rect_.width;
		status.active_height = active_rect_.height;
		status.width = output_desc_.texture.width;
		status.height = output_desc_.texture.height;
		status.format = output_desc_.texture.format;
		status.fps = fps_;
		status.frames_sent = frames_sent_;
		status.frames_skipped = frames_skipped_;
		status.last_copy_cpu_us = last_copy_cpu_us_;
		status.last_mutex_wait_us = last_mutex_wait_us_;
		status.last_bridge_us = last_bridge_us_;
		status.uses_bridge = uses_bridge_;
		return status;
	}

	bool CaptureManager::IsActive() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return state_ == CaptureState::Active;
	}

	uint32_t CaptureManager::SourceId() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return source_id_;
	}

	resource_view CaptureManager::OutputView() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return output_srv_;
	}

	resource_desc CaptureManager::OutputDesc() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return output_desc_;
	}
}
