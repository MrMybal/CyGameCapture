// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "FullscreenPass.hpp"
#include "Addon/Log.hpp"

#include <d3dcompiler.h>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		/**
		 * The fullscreen triangle every pass uses. Three vertices, no buffer, no input layout: the
		 * positions and texture coordinates come from SV_VertexID. Kept separate from the pixel shaders
		 * so each pass only has to provide its own pixel entry point.
		 */
		constexpr const char *kVertexShader = R"HLSL(
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut VSMain(uint id : SV_VertexID)
{
	VSOut o;
	float2 uv = float2((id << 1) & 2, id & 2);
	o.uv = uv;
	o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return o;
}
)HLSL";
	}

	bool CompileHlsl(const char *hlsl, const char *entry_point, const char *target, std::string &out_bytecode, std::string &error)
	{
		ID3DBlob *code = nullptr;
		ID3DBlob *errors = nullptr;
		const HRESULT hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, entry_point, target,
			D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);

		if (FAILED(hr) || code == nullptr)
		{
			error = errors != nullptr ? static_cast<const char *>(errors->GetBufferPointer()) : "D3DCompile failed";
			if (errors != nullptr)
				errors->Release();
			if (code != nullptr)
				code->Release();
			return false;
		}

		out_bytecode.assign(static_cast<const char *>(code->GetBufferPointer()), code->GetBufferSize());
		code->Release();
		if (errors != nullptr)
			errors->Release();
		return true;
	}

	FullscreenPass::~FullscreenPass()
	{
		Destroy();
	}

	bool FullscreenPass::Create(device *device, const char *hlsl, const char *pixel_entry,
		uint32_t input_count, uint32_t constant_dwords, format render_target_format, std::string &error)
	{
		Destroy();

		if (device == nullptr || hlsl == nullptr || pixel_entry == nullptr)
		{
			error = "FullscreenPass::Create called with no device or no shader";
			return false;
		}

		// Shader model 5.0: understood by the D3D11 and D3D12 back ends, and by every GPU that can run
		// this add-on at all. DXBC, not DXIL: ReShade's Direct3D back ends take DXBC.
		std::string vertex_bytecode;
		std::string pixel_bytecode;
		if (!CompileHlsl(kVertexShader, "VSMain", "vs_5_0", vertex_bytecode, error))
			return false;
		if (!CompileHlsl(hlsl, pixel_entry, "ps_5_0", pixel_bytecode, error))
			return false;

		device_ = device;
		render_target_format_ = render_target_format;
		input_count_ = input_count;
		constant_dwords_ = constant_dwords;

		// --- pipeline layout ----------------------------------------------------------------------
		const descriptor_range srv_range = [input_count]() {
			descriptor_range range;
			range.binding = 0;
			range.dx_register_index = 0;
			range.dx_register_space = 0;
			range.count = input_count;
			range.visibility = shader_stage::pixel;
			range.type = descriptor_type::shader_resource_view;
			return range;
		}();

		const descriptor_range sampler_range = []() {
			descriptor_range range;
			range.binding = 0;
			range.dx_register_index = 0;
			range.dx_register_space = 0;
			range.count = 1;
			range.visibility = shader_stage::pixel;
			range.type = descriptor_type::sampler;
			return range;
		}();

		constant_range constants = {};
		constants.binding = 0;
		constants.dx_register_index = 0;
		constants.dx_register_space = 0;
		constants.count = constant_dwords;
		constants.visibility = shader_stage::pixel;

		std::vector<pipeline_layout_param> params;
		params.emplace_back(srv_range);
		params.emplace_back(sampler_range);
		if (constant_dwords != 0)
			params.emplace_back(constants);

		if (!device_->create_pipeline_layout(static_cast<uint32_t>(params.size()), params.data(), &layout_))
		{
			error = "create_pipeline_layout failed";
			Destroy();
			return false;
		}

		// --- sampler ------------------------------------------------------------------------------
		// Linear + clamp. A depth pass reads at exactly one texel per pixel so filtering never kicks in,
		// and a difference pass wants the same sampling as the source; clamping avoids wrapping at edges.
		sampler_desc sampler = {};
		sampler.filter = filter_mode::min_mag_mip_linear;
		sampler.address_u = texture_address_mode::clamp;
		sampler.address_v = texture_address_mode::clamp;
		sampler.address_w = texture_address_mode::clamp;
		sampler.max_anisotropy = 1;
		sampler.min_lod = 0.0f;
		sampler.max_lod = FLT_MAX;
		if (!device_->create_sampler(sampler, &sampler_))
		{
			error = "create_sampler failed";
			Destroy();
			return false;
		}

		// --- pipeline -----------------------------------------------------------------------------
		shader_desc vertex_shader = {};
		vertex_shader.code = vertex_bytecode.data();
		vertex_shader.code_size = vertex_bytecode.size();

		shader_desc pixel_shader = {};
		pixel_shader.code = pixel_bytecode.data();
		pixel_shader.code_size = pixel_bytecode.size();

		blend_desc blend = {};   // defaults: no blending, write all channels

		rasterizer_desc rasterizer = {};
		rasterizer.cull_mode = cull_mode::none;   // the fullscreen triangle has no meaningful winding

		depth_stencil_desc depth_stencil = {};
		depth_stencil.depth_enable = false;       // there is no depth target bound
		depth_stencil.depth_write_mask = false;
		depth_stencil.stencil_enable = false;

		primitive_topology topology = primitive_topology::triangle_list;
		format render_target_formats[1] = { render_target_format };
		uint32_t sample_mask = 0xFFFFFFFF;
		uint32_t sample_count = 1;
		uint32_t viewport_count = 1;

		const pipeline_subobject subobjects[] = {
			{ pipeline_subobject_type::vertex_shader, 1, &vertex_shader },
			{ pipeline_subobject_type::pixel_shader, 1, &pixel_shader },
			{ pipeline_subobject_type::blend_state, 1, &blend },
			{ pipeline_subobject_type::rasterizer_state, 1, &rasterizer },
			{ pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil },
			{ pipeline_subobject_type::primitive_topology, 1, &topology },
			{ pipeline_subobject_type::render_target_formats, 1, render_target_formats },
			{ pipeline_subobject_type::sample_mask, 1, &sample_mask },
			{ pipeline_subobject_type::sample_count, 1, &sample_count },
			{ pipeline_subobject_type::viewport_count, 1, &viewport_count },
		};

		if (!device_->create_pipeline(layout_, static_cast<uint32_t>(std::size(subobjects)), subobjects, &pipeline_))
		{
			error = "create_pipeline failed (the graphics API back end may not support this pass)";
			Destroy();
			return false;
		}

		return true;
	}

	void FullscreenPass::Destroy()
	{
		if (device_ == nullptr)
			return;
		if (pipeline_ != 0)
			device_->destroy_pipeline(pipeline_);
		if (layout_ != 0)
			device_->destroy_pipeline_layout(layout_);
		if (sampler_ != 0)
			device_->destroy_sampler(sampler_);
		pipeline_ = {};
		layout_ = {};
		sampler_ = {};
		render_target_format_ = format::unknown;
		input_count_ = 0;
		constant_dwords_ = 0;
		device_ = nullptr;
	}

	void FullscreenPass::Draw(command_list *cmd, resource_view target,
		const resource_view *inputs, uint32_t input_count,
		const void *constants, uint32_t constant_dwords, uint32_t width, uint32_t height) const
	{
		if (cmd == nullptr || pipeline_ == 0 || target == 0 || inputs == nullptr || input_count != input_count_)
			return;

		render_pass_render_target_desc render_target = {};
		render_target.view = target;
		render_target.load_op = render_pass_load_op::discard;   // every pixel is written by the pass
		render_target.store_op = render_pass_store_op::store;

		cmd->begin_render_pass(1, &render_target, nullptr);

		cmd->bind_pipeline(pipeline_stage::all_graphics, pipeline_);

		const viewport view = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		cmd->bind_viewports(0, 1, &view);
		const rect scissor = { 0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height) };
		cmd->bind_scissor_rects(0, 1, &scissor);

		descriptor_table_update srv_update = {};
		srv_update.binding = 0;
		srv_update.count = input_count;
		srv_update.type = descriptor_type::shader_resource_view;
		srv_update.descriptors = inputs;
		cmd->push_descriptors(shader_stage::pixel, layout_, 0, srv_update);

		descriptor_table_update sampler_update = {};
		sampler_update.binding = 0;
		sampler_update.count = 1;
		sampler_update.type = descriptor_type::sampler;
		sampler_update.descriptors = &sampler_;
		cmd->push_descriptors(shader_stage::pixel, layout_, 1, sampler_update);

		if (constant_dwords_ != 0 && constants != nullptr && constant_dwords == constant_dwords_)
			cmd->push_constants(shader_stage::pixel, layout_, 2, 0, constant_dwords_, constants);

		cmd->draw(3, 1, 0, 0);

		cmd->end_render_pass();
	}
}
