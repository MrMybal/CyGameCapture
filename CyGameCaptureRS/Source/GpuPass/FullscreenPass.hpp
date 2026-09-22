// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — one fullscreen pixel-shader pass, through reshade::api.
//
// Until now the add-on only ever issued copies. Producing a *derived* stream needs real shader work:
// linearising a depth buffer into something that can be shared and recorded, or subtracting the clean
// scene from the final image to isolate the HUD with an alpha channel. Both are the same shape — read
// one or two textures, write one render target, no vertex buffer — so they share this.
//
// Everything goes through reshade::api, so the same code runs on D3D10/11/12 (and would on Vulkan and
// OpenGL, which are not wired up in this add-on yet). The shaders are compiled from HLSL to DXBC at
// run time with D3DCompile, which is what the Direct3D back ends of ReShade expect.
//
// The geometry is the usual fullscreen triangle generated from SV_VertexID: no vertex buffer, no input
// layout, one draw of three vertices.
#pragma once

#include <reshade.hpp>

#include <cstdint>
#include <string>

namespace cygc
{
	/**
	 * A compiled pass, ready to be drawn any number of times.
	 *
	 * The layout is fixed and simple on purpose:
	 *   param 0 : the input textures,  t0..tN   (push descriptors, pixel stage)
	 *   param 1 : one linear clamp sampler, s0  (push descriptors, pixel stage)
	 *   param 2 : the constant block, b0         (push constants, pixel stage)
	 */
	class FullscreenPass
	{
	public:
		FullscreenPass() = default;
		~FullscreenPass();

		FullscreenPass(const FullscreenPass &) = delete;
		FullscreenPass &operator=(const FullscreenPass &) = delete;

		/**
		 * Compiles the shader and builds the pipeline for one render target format.
		 *
		 * `hlsl` must contain the pixel shader entry point given in `pixel_entry`; the vertex shader is
		 * supplied by this class. `constant_dwords` is the size of the b0 block in 32-bit words, 0 when
		 * the pass needs none.
		 */
		bool Create(reshade::api::device *device, const char *hlsl, const char *pixel_entry,
			uint32_t input_count, uint32_t constant_dwords, reshade::api::format render_target_format,
			std::string &error);

		void Destroy();
		bool IsValid() const { return pipeline_ != 0; }

		/** Format the pipeline was built for; a different target needs a different pass. */
		reshade::api::format RenderTargetFormat() const { return render_target_format_; }

		/**
		 * Records the pass. `target` must already be in resource_usage::render_target and the inputs in
		 * resource_usage::shader_resource: the caller owns the transitions, because it knows what the
		 * resources were doing before and after.
		 */
		void Draw(reshade::api::command_list *cmd, reshade::api::resource_view target,
			const reshade::api::resource_view *inputs, uint32_t input_count,
			const void *constants, uint32_t constant_dwords, uint32_t width, uint32_t height) const;

	private:
		reshade::api::device *device_ = nullptr;
		reshade::api::pipeline pipeline_ = {};
		reshade::api::pipeline_layout layout_ = {};
		reshade::api::sampler sampler_ = {};
		reshade::api::format render_target_format_ = reshade::api::format::unknown;
		uint32_t input_count_ = 0;
		uint32_t constant_dwords_ = 0;
	};

	/**
	 * Compiles HLSL to DXBC with D3DCompile. Exposed because the buffer inspector will want to report
	 * compilation errors of user-provided passes later on.
	 */
	bool CompileHlsl(const char *hlsl, const char *entry_point, const char *target,
		std::string &out_bytecode, std::string &error);
}
