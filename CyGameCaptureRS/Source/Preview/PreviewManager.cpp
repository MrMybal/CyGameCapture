// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PreviewManager.hpp"
#include "GpuPass/PassInput.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "Addon/Log.hpp"

using namespace reshade::api;

namespace cygc
{
	PreviewManager::PreviewManager(device *device) :
		device_(device)
	{
	}

	PreviewManager::~PreviewManager()
	{
		DestroyTexture();
	}

	bool PreviewManager::EnsureTexture(const resource_desc &source_desc)
	{
		const format out_format = ToOutputFormat(source_desc.texture.format);
		if (texture_ != 0 &&
			texture_desc_.texture.width == source_desc.texture.width &&
			texture_desc_.texture.height == source_desc.texture.height &&
			texture_desc_.texture.format == out_format)
			return true;

		DestroyTexture();

		const resource_desc desc(source_desc.texture.width, source_desc.texture.height, 1, 1, out_format, 1,
			memory_heap::default_, resource_usage::shader_resource | resource_usage::copy_dest | resource_usage::resolve_dest);

		if (!device_->create_resource(desc, nullptr, resource_usage::copy_dest, &texture_))
		{
			last_error_ = std::string("Failed to create preview texture (") + FormatName(out_format) + ")";
			log::Error("%s", last_error_.c_str());
			texture_ = {};
			return false;
		}
		texture_desc_ = device_->get_resource_desc(texture_);

		if (!device_->create_resource_view(texture_, resource_usage::shader_resource, resource_view_desc(texture_desc_.texture.format), &texture_view_))
		{
			last_error_ = "Failed to create preview shader resource view";
			log::Error("%s", last_error_.c_str());
			device_->destroy_resource(texture_);
			texture_ = {};
			texture_view_ = {};
			return false;
		}

		log::Verbose("Preview texture created %ux%u %s", texture_desc_.texture.width, texture_desc_.texture.height, FormatName(texture_desc_.texture.format));
		last_error_.clear();
		return true;
	}

	void PreviewManager::DestroyTexture()
	{
		if (view_ == texture_view_)
			view_ = {};
		if (texture_view_ != 0)
			device_->destroy_resource_view(texture_view_);
		if (texture_ != 0)
			device_->destroy_resource(texture_);
		texture_view_ = {};
		texture_ = {};
		texture_desc_ = {};
	}

	void PreviewManager::Update(command_queue *queue, const TrackedResource &source)
	{
		if (frozen_ && !external_)
			return; // keep the frozen image (own texture)

		if (!source.IsTexture2D() || source.Width() == 0 || source.Height() == 0)
			return;
		if (source.IsDepthStencil() || IsDepthStencilFormat(source.Format()))
		{
			last_error_ = "Depth preview is not implemented yet (milestone 3)";
			return;
		}
		if (IsBlockCompressedFormat(source.Format()))
		{
			last_error_ = "Block compressed formats cannot be previewed";
			return;
		}

		if (!EnsureTexture(source.desc))
			return;

		command_list *const cmd_list = queue->get_immediate_command_list();
		const resource_usage source_state = GuessSourceState(source);

		if (source.IsMultisampled())
		{
			cmd_list->barrier(source.handle, source_state, resource_usage::resolve_source);
			cmd_list->barrier(texture_, resource_usage::shader_resource, resource_usage::resolve_dest);
			cmd_list->resolve_texture_region(source.handle, 0, nullptr, texture_, 0, 0, 0, 0, texture_desc_.texture.format);
			cmd_list->barrier(texture_, resource_usage::resolve_dest, resource_usage::shader_resource);
			cmd_list->barrier(source.handle, resource_usage::resolve_source, source_state);
		}
		else
		{
			cmd_list->barrier(source.handle, source_state, resource_usage::copy_source);
			cmd_list->barrier(texture_, resource_usage::shader_resource, resource_usage::copy_dest);
			cmd_list->copy_texture_region(source.handle, 0, nullptr, texture_, 0, nullptr);
			cmd_list->barrier(texture_, resource_usage::copy_dest, resource_usage::shader_resource);
			cmd_list->barrier(source.handle, resource_usage::copy_source, source_state);
		}

		view_ = texture_view_;
		desc_ = texture_desc_;
		external_ = false;
		source_id_ = source.id;
		last_update_frame_++;
		last_error_.clear();
	}

	void PreviewManager::UseExternalView(resource_view view, const resource_desc &desc, uint32_t source_id)
	{
		if (frozen_)
			return;
		view_ = view;
		desc_ = desc;
		external_ = true;
		source_id_ = source_id;
		last_update_frame_++;
	}

	void PreviewManager::Clear()
	{
		view_ = {};
		desc_ = {};
		external_ = false;
		source_id_ = 0;
	}
}
