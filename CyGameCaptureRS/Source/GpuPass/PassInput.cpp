// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PassInput.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "Addon/Log.hpp"

using namespace reshade::api;

namespace cygc
{
	resource_usage GuessSourceState(const TrackedResource &res)
	{
		if (res.is_backbuffer)
			return resource_usage::present;
		if (res.IsDepthStencil())
			return resource_usage::depth_stencil;
		if (res.IsColorTarget())
			return resource_usage::render_target;
		if (res.IsUnorderedAccess())
			return resource_usage::unordered_access;
		return resource_usage::shader_resource;
	}

	bool PrepareInput(device *device, PassInput &input, const TrackedResource &source,
		command_list *cmd_list, const ResourceNotice &on_created)
	{
		const format view_format = format_to_default_typed(source.Format());
		const bool source_is_samplable = (source.desc.usage & resource_usage::shader_resource) == resource_usage::shader_resource;

		if (source_is_samplable)
		{
			if (input.view != 0 && input.viewed == source.handle)
				return true;

			DestroyInput(device, input, on_created);
			if (!device->create_resource_view(source.handle, resource_usage::shader_resource, resource_view_desc(view_format), &input.view))
			{
				input.view = {};
				return false;
			}
			input.viewed = source.handle;
			return true;
		}

		// Not samplable (a back buffer, most of the time): keep our own copy and sample that.
		const resource_desc wanted(source.Width(), source.Height(), 1, 1, format_to_typeless(source.Format()), 1,
			memory_heap::default_, resource_usage::copy_dest | resource_usage::shader_resource);

		if (input.copy == 0 || input.copy_desc.texture.width != wanted.texture.width ||
			input.copy_desc.texture.height != wanted.texture.height || input.copy_desc.texture.format != wanted.texture.format)
		{
			DestroyInput(device, input, on_created);
			if (!device->create_resource(wanted, nullptr, resource_usage::copy_dest, &input.copy))
			{
				input.copy = {};
				log::Verbose("Could not create the readable copy of #%03u (%ux%u %s)", source.id,
					source.Width(), source.Height(), FormatName(source.Format()));
				return false;
			}
			if (on_created)
				on_created(input.copy);
			input.copy_desc = device->get_resource_desc(input.copy);
			if (!device->create_resource_view(input.copy, resource_usage::shader_resource, resource_view_desc(view_format), &input.view))
			{
				DestroyInput(device, input, on_created);
				return false;
			}
			input.viewed = input.copy;
		}

		const resource_usage source_state = GuessSourceState(source);
		cmd_list->barrier(source.handle, source_state, resource_usage::copy_source);
		cmd_list->barrier(input.copy, resource_usage::shader_resource, resource_usage::copy_dest);
		cmd_list->copy_texture_region(source.handle, 0, nullptr, input.copy, 0, nullptr);
		cmd_list->barrier(input.copy, resource_usage::copy_dest, resource_usage::shader_resource);
		cmd_list->barrier(source.handle, resource_usage::copy_source, source_state);
		return true;
	}

	void DestroyInput(device *device, PassInput &input, const ResourceNotice &on_destroyed)
	{
		if (input.view != 0)
			device->destroy_resource_view(input.view);
		if (input.copy != 0)
		{
			if (on_destroyed)
				on_destroyed(input.copy);
			device->destroy_resource(input.copy);
		}
		input = PassInput {};
	}
}
