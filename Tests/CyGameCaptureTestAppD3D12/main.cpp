// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureTestAppD3D12 — the D3D12 twin of CyGameCaptureTestApp, used to validate the
// CyGameCaptureRS D3D11On12 bridge.
//
// Frame structure (same idea as the D3D11 app, so the inspector shows comparable resources):
//   1. Scene   : animated blobs into an R16G16B16A16_FLOAT render target
//   2. Tonemap : fullscreen pass HDR -> R8G8B8A8_UNORM "clean scene" render target
//   3. Composite: clean scene blitted onto the back buffer
//   4. HUD     : coloured quads drawn on the back buffer only
//
// The player sees 3+4, CyGameCaptureRS should be able to stream 2 (clean, no HUD) through the bridge.
#include <Windows.h>
#include <CyGameCaptureBranding.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace
{
	constexpr UINT kWidth = 1280;
	constexpr UINT kHeight = 720;
	constexpr UINT kFrameCount = 2;

	constexpr DXGI_FORMAT kSceneFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT kCleanFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

	const char *const kShaderSource = R"HLSL(
cbuffer Constants : register(b0)
{
	float4 params;   // x = time, y = aspect
	float4 color;    // quad: xy = center, zw = half size | hud: rgb = colour
};
Texture2D    tex0 : register(t0);
SamplerState samp : register(s0);

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut VSFullscreen(uint id : SV_VertexID)
{
	VSOut o;
	float2 uv = float2((id << 1) & 2, id & 2);
	o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
	o.uv = uv;
	return o;
}

VSOut VSQuad(uint id : SV_VertexID)
{
	VSOut o;
	float2 corners[6] = { float2(-1,-1), float2(-1,1), float2(1,-1), float2(1,-1), float2(-1,1), float2(1,1) };
	float2 c = corners[id];
	o.pos = float4(color.xy + c * color.zw, 0, 1);
	o.uv = c * 0.5 + 0.5;
	return o;
}

float4 PSScene(VSOut i, out float out_depth : SV_Depth) : SV_Target
{
	float t = params.x;
	float2 p = (i.uv - 0.5) * float2(params.y, 1.0);
	float3 col = float3(0.02, 0.03, 0.06) + float3(0.1, 0.2, 0.4) * i.uv.y;
	for (int k = 0; k < 5; ++k)
	{
		float fk = k;
		float2 c = 0.35 * float2(cos(t * 0.7 + fk * 1.3), sin(t * 0.9 + fk * 2.1));
		float d = length(p - c);
		float3 bc = 0.5 + 0.5 * cos(fk * 1.7 + float3(0, 2, 4));
		col += bc * 2.5 * exp(-d * d * 60.0);
	}
	float a = atan2(p.y, p.x) + t;
	col += 0.6 * step(0.9, frac(a * 3.0 / 6.2831)) * smoothstep(0.45, 0.2, length(p));

	// Depth: the nearest blob wins, the background falls back to a gradient. Standard Z (0 near, 1 far).
	float nearest = 1.0;
	for (int m = 0; m < 5; ++m)
	{
		float fm = m;
		float2 c = 0.35 * float2(cos(t * 0.7 + fm * 1.3), sin(t * 0.9 + fm * 2.1));
		float blob = smoothstep(0.18, 0.0, length(p - c));
		nearest = min(nearest, lerp(1.0, 0.05 + 0.15 * fm, blob));
	}
	out_depth = min(nearest, 0.55 + 0.45 * i.uv.y);

	return float4(col, 1);
}

// `color.xy` scales the texture coordinates so a pass reads only the part of its source that the
// previous pass rendered into: that is what dynamic resolution looks like from the inside.
float4 PSTonemap(VSOut i) : SV_Target
{
	float3 hdr = tex0.Sample(samp, i.uv * color.xy).rgb;
	float3 mapped = hdr / (1.0 + hdr);
	return float4(pow(mapped, 1.0 / 2.2), 1);
}

float4 PSCopy(VSOut i) : SV_Target
{
	return tex0.Sample(samp, i.uv * color.xy);
}

float4 PSHud(VSOut i) : SV_Target
{
	float2 b = abs(i.uv - 0.5) * 2.0;
	float border = step(0.9, max(b.x, b.y));
	return float4(lerp(params.rgb, float3(1, 1, 1), border), 1);
}
)HLSL";

	struct Constants
	{
		float params[4];
		float color[4];
	};

	void ThrowIfFailed(HRESULT hr, const char *what)
	{
		if (FAILED(hr))
		{
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "%s failed: 0x%08X", what, static_cast<unsigned int>(hr));
			throw std::runtime_error(buffer);
		}
	}

	struct RenderTarget
	{
		ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
		UINT srv_index = 0;
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_RENDER_TARGET;
	};

	// ---------------------------------------------------------------------------------------------
	ComPtr<ID3D12Device> g_device;
	ComPtr<ID3D12CommandQueue> g_queue;
	ComPtr<IDXGISwapChain3> g_swapchain;
	ComPtr<ID3D12DescriptorHeap> g_rtv_heap;
	ComPtr<ID3D12DescriptorHeap> g_srv_heap;
	ComPtr<ID3D12CommandAllocator> g_allocators[kFrameCount];
	ComPtr<ID3D12GraphicsCommandList> g_cmd;
	ComPtr<ID3D12RootSignature> g_root;
	ComPtr<ID3D12PipelineState> g_pso_scene, g_pso_tonemap, g_pso_copy, g_pso_hud;
	ComPtr<ID3D12Resource> g_backbuffers[kFrameCount];
	D3D12_CPU_DESCRIPTOR_HANDLE g_backbuffer_rtv[kFrameCount] = {};
	RenderTarget g_scene, g_clean;
	ComPtr<ID3D12DescriptorHeap> g_dsv_heap;
	ComPtr<ID3D12Resource> g_depth;
	D3D12_CPU_DESCRIPTOR_HANDLE g_dsv = {};
	ComPtr<ID3D12Fence> g_fence;
	UINT64 g_fence_values[kFrameCount] = {};   // last value signalled for the work of each frame slot
	UINT64 g_next_fence_value = 1;             // strictly monotonic: Signal() must never go backwards
	HANDLE g_fence_event = nullptr;
	UINT g_frame_index = 0;
	UINT g_rtv_size = 0, g_srv_size = 0;
	bool g_running = true;

	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_DESTROY:
			g_running = false;
			PostQuitMessage(0);
			return 0;
		case WM_KEYDOWN:
			if (wparam == VK_ESCAPE)
				DestroyWindow(hwnd);
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	ComPtr<ID3DBlob> Compile(const char *entry, const char *target)
	{
		ComPtr<ID3DBlob> blob, errors;
		const HRESULT hr = D3DCompile(kShaderSource, strlen(kShaderSource), "test.hlsl", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
		if (FAILED(hr))
		{
			MessageBoxA(nullptr, errors ? static_cast<const char *>(errors->GetBufferPointer()) : "unknown error", "Shader compilation failed", MB_ICONERROR);
			throw std::runtime_error("shader");
		}
		return blob;
	}

	ComPtr<ID3D12PipelineState> CreatePSO(ID3DBlob *vs, ID3DBlob *ps, DXGI_FORMAT rtv_format,
		DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = g_root.Get();
		desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.RasterizerState.DepthClipEnable = TRUE;
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.DepthStencilState.DepthEnable = dsv_format != DXGI_FORMAT_UNKNOWN;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.DSVFormat = dsv_format;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = rtv_format;
		desc.SampleDesc.Count = 1;

		ComPtr<ID3D12PipelineState> pso;
		ThrowIfFailed(g_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)), "CreateGraphicsPipelineState");
		return pso;
	}

	void CreateRenderTarget(RenderTarget &rt, UINT width, UINT height, DXGI_FORMAT format, UINT rtv_slot, UINT srv_slot, const wchar_t *name)
	{
		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clear = {};
		clear.Format = format;

		ThrowIfFailed(g_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clear, IID_PPV_ARGS(&rt.resource)), "CreateCommittedResource(render target)");
		rt.resource->SetName(name);

		rt.rtv = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
		rt.rtv.ptr += static_cast<SIZE_T>(rtv_slot) * g_rtv_size;
		g_device->CreateRenderTargetView(rt.resource.Get(), nullptr, rt.rtv);

		D3D12_CPU_DESCRIPTOR_HANDLE srv = g_srv_heap->GetCPUDescriptorHandleForHeapStart();
		srv.ptr += static_cast<SIZE_T>(srv_slot) * g_srv_size;
		g_device->CreateShaderResourceView(rt.resource.Get(), nullptr, srv);
		rt.srv_index = srv_slot;
		rt.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	void Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES &current, D3D12_RESOURCE_STATES next)
	{
		if (current == next)
			return;
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = current;
		barrier.Transition.StateAfter = next;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		g_cmd->ResourceBarrier(1, &barrier);
		current = next;
	}

	/** Blocks until everything submitted so far has finished. Used at init and at shutdown. */
	void WaitForGpu()
	{
		if (!g_queue || !g_fence)
			return;
		const UINT64 value = g_next_fence_value++;
		if (FAILED(g_queue->Signal(g_fence.Get(), value)))
			return;
		if (g_fence->GetCompletedValue() < value)
		{
			if (SUCCEEDED(g_fence->SetEventOnCompletion(value, g_fence_event)))
				WaitForSingleObject(g_fence_event, 2000);
		}
		for (UINT i = 0; i < kFrameCount; ++i)
			g_fence_values[i] = value;
	}

	void InitD3D(HWND hwnd)
	{
		UINT factory_flags = 0;
		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
		ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)), "D3D12CreateDevice");

		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed(g_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_queue)), "CreateCommandQueue");

		DXGI_SWAP_CHAIN_DESC1 scd = {};
		scd.BufferCount = kFrameCount;
		scd.Width = kWidth;
		scd.Height = kHeight;
		scd.Format = kBackBufferFormat;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		scd.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapchain1;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(g_queue.Get(), hwnd, &scd, nullptr, nullptr, &swapchain1), "CreateSwapChainForHwnd");
		ThrowIfFailed(swapchain1.As(&g_swapchain), "IDXGISwapChain3");
		g_frame_index = g_swapchain->GetCurrentBackBufferIndex();

		D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
		rtv_desc.NumDescriptors = kFrameCount + 2;   // back buffers + scene + clean
		rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		ThrowIfFailed(g_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&g_rtv_heap)), "CreateDescriptorHeap(RTV)");
		g_rtv_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC srv_desc = {};
		srv_desc.NumDescriptors = 2;                 // scene + clean
		srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(g_device->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&g_srv_heap)), "CreateDescriptorHeap(SRV)");
		g_srv_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		for (UINT i = 0; i < kFrameCount; ++i)
		{
			ThrowIfFailed(g_swapchain->GetBuffer(i, IID_PPV_ARGS(&g_backbuffers[i])), "GetBuffer");
			g_backbuffer_rtv[i] = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
			g_backbuffer_rtv[i].ptr += static_cast<SIZE_T>(i) * g_rtv_size;
			g_device->CreateRenderTargetView(g_backbuffers[i].Get(), nullptr, g_backbuffer_rtv[i]);
			ThrowIfFailed(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_allocators[i])), "CreateCommandAllocator");
		}

		// Root signature: 8 root constants (b0) + one descriptor table with one SRV (t0) + static sampler
		D3D12_DESCRIPTOR_RANGE range = {};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = 0;

		D3D12_ROOT_PARAMETER params[2] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.Num32BitValues = 8;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &range;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC root_desc = {};
		root_desc.NumParameters = 2;
		root_desc.pParameters = params;
		root_desc.NumStaticSamplers = 1;
		root_desc.pStaticSamplers = &sampler;
		root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature, error;
		ThrowIfFailed(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "D3D12SerializeRootSignature");
		ThrowIfFailed(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_root)), "CreateRootSignature");

		const ComPtr<ID3DBlob> vs_fullscreen = Compile("VSFullscreen", "vs_5_0");
		const ComPtr<ID3DBlob> vs_quad = Compile("VSQuad", "vs_5_0");
		// Depth buffer, so the add-on has a real one to linearise. It is created with the shader
		// resource flag left off on purpose: that is the common case in a game, and it exercises the
		// readable-copy path of the capture pass.
		{
			D3D12_DESCRIPTOR_HEAP_DESC dsv_desc = {};
			dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsv_desc.NumDescriptors = 1;
			ThrowIfFailed(g_device->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&g_dsv_heap)), "CreateDescriptorHeap(DSV)");

			D3D12_HEAP_PROPERTIES heap = {};
			heap.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC depth_desc = {};
			depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			depth_desc.Width = kWidth;
			depth_desc.Height = kHeight;
			depth_desc.DepthOrArraySize = 1;
			depth_desc.MipLevels = 1;
			depth_desc.Format = kDepthFormat;
			depth_desc.SampleDesc.Count = 1;
			depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE clear = {};
			clear.Format = kDepthFormat;
			clear.DepthStencil.Depth = 1.0f;

			ThrowIfFailed(g_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &depth_desc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&g_depth)), "CreateCommittedResource(depth)");
			g_depth->SetName(L"SceneDepth");

			g_dsv = g_dsv_heap->GetCPUDescriptorHandleForHeapStart();
			g_device->CreateDepthStencilView(g_depth.Get(), nullptr, g_dsv);
		}

		g_pso_scene = CreatePSO(vs_fullscreen.Get(), Compile("PSScene", "ps_5_0").Get(), kSceneFormat, kDepthFormat);
		g_pso_tonemap = CreatePSO(vs_fullscreen.Get(), Compile("PSTonemap", "ps_5_0").Get(), kCleanFormat);
		g_pso_copy = CreatePSO(vs_fullscreen.Get(), Compile("PSCopy", "ps_5_0").Get(), kBackBufferFormat);
		g_pso_hud = CreatePSO(vs_quad.Get(), Compile("PSHud", "ps_5_0").Get(), kBackBufferFormat);

		CreateRenderTarget(g_scene, kWidth, kHeight, kSceneFormat, kFrameCount + 0, 0, L"SceneColorHDR");
		CreateRenderTarget(g_clean, kWidth, kHeight, kCleanFormat, kFrameCount + 1, 1, L"SceneCleanLDR");

		ThrowIfFailed(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_allocators[g_frame_index].Get(), nullptr, IID_PPV_ARGS(&g_cmd)), "CreateCommandList");
		ThrowIfFailed(g_cmd->Close(), "Close");

		ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)), "CreateFence");
		g_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		WaitForGpu();
	}

	void SetConstants(const Constants &c)
	{
		g_cmd->SetGraphicsRoot32BitConstants(0, 8, &c, 0);
	}

	void FullscreenPass(D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT width, UINT height, ID3D12PipelineState *pso, const RenderTarget *input, const Constants &constants,
		const D3D12_CPU_DESCRIPTOR_HANDLE *dsv = nullptr)
	{
		g_cmd->OMSetRenderTargets(1, &rtv, FALSE, dsv);
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		D3D12_RECT scissor = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		g_cmd->RSSetViewports(1, &viewport);
		g_cmd->RSSetScissorRects(1, &scissor);
		g_cmd->SetPipelineState(pso);
		SetConstants(constants);
		if (input != nullptr)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE srv = g_srv_heap->GetGPUDescriptorHandleForHeapStart();
			srv.ptr += static_cast<UINT64>(input->srv_index) * g_srv_size;
			g_cmd->SetGraphicsRootDescriptorTable(1, srv);
		}
		g_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		g_cmd->DrawInstanced(3, 1, 0, 0);
	}

	void HudQuad(float cx, float cy, float hw, float hh, float r, float g, float b)
	{
		Constants c = {};
		c.params[0] = r; c.params[1] = g; c.params[2] = b;
		c.color[0] = cx; c.color[1] = cy; c.color[2] = hw; c.color[3] = hh;
		SetConstants(c);
		g_cmd->DrawInstanced(6, 1, 0, 0);
	}

	void RenderFrame(float time)
	{
		ThrowIfFailed(g_allocators[g_frame_index]->Reset(), "allocator Reset");
		ThrowIfFailed(g_cmd->Reset(g_allocators[g_frame_index].Get(), nullptr), "cmd Reset");

		g_cmd->SetGraphicsRootSignature(g_root.Get());
		ID3D12DescriptorHeap *heaps[] = { g_srv_heap.Get() };
		g_cmd->SetDescriptorHeaps(1, heaps);

		// Dynamic resolution the way engines do it: the render targets keep their full size and the
		// scene is drawn into a sub-rectangle, then upscaled onto the back buffer. Nothing is ever
		// reallocated, so only the viewport says how much of each target holds the frame.
		const float resolution_scale = 0.75f + 0.25f * sinf(time * 0.6f);
		const UINT scene_width = static_cast<UINT>(kWidth * resolution_scale);
		const UINT scene_height = static_cast<UINT>(kHeight * resolution_scale);
		const float source_scale = static_cast<float>(scene_width) / static_cast<float>(kWidth);

		Constants scene_constants = {};
		scene_constants.params[0] = time;
		scene_constants.params[1] = static_cast<float>(kWidth) / static_cast<float>(kHeight);

		// What a pass has to read of its source; the scene pass itself reads nothing
		Constants scaled_constants = scene_constants;
		scaled_constants.color[0] = source_scale;
		scaled_constants.color[1] = source_scale;

		// 1. Scene (HDR)
		Transition(g_scene.resource.Get(), g_scene.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		g_cmd->ClearRenderTargetView(g_scene.rtv, clear, 0, nullptr);
		g_cmd->ClearDepthStencilView(g_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		FullscreenPass(g_scene.rtv, scene_width, scene_height, g_pso_scene.Get(), nullptr, scene_constants, &g_dsv);

		// 2. Tonemap -> clean LDR scene (the buffer CyGameCaptureRS should stream)
		Transition(g_scene.resource.Get(), g_scene.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Transition(g_clean.resource.Get(), g_clean.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FullscreenPass(g_clean.rtv, scene_width, scene_height, g_pso_tonemap.Get(), &g_scene, scaled_constants);

		// 3. Composite onto the back buffer
		D3D12_RESOURCE_STATES backbuffer_state = D3D12_RESOURCE_STATE_PRESENT;
		Transition(g_backbuffers[g_frame_index].Get(), backbuffer_state, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Transition(g_clean.resource.Get(), g_clean.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// Upscale onto the full-size back buffer, exactly as a game ends a dynamically scaled frame
		FullscreenPass(g_backbuffer_rtv[g_frame_index], kWidth, kHeight, g_pso_copy.Get(), &g_clean, scaled_constants);

		// 4. HUD on the back buffer only
		g_cmd->SetPipelineState(g_pso_hud.Get());
		HudQuad(-0.72f, 0.82f, 0.25f, 0.06f, 0.9f, 0.2f, 0.2f);
		HudQuad(-0.72f - 0.25f * (1.0f - (0.5f + 0.5f * sinf(time))), 0.82f, 0.25f * (0.5f + 0.5f * sinf(time)), 0.045f, 0.2f, 0.9f, 0.2f);
		HudQuad(0.75f, 0.80f, 0.18f, 0.16f, 0.1f, 0.3f, 0.8f);
		HudQuad(0.0f, -0.88f, 0.55f, 0.05f, 0.85f, 0.75f, 0.1f);
		HudQuad(0.0f, 0.0f, 0.012f, 0.012f, 1.0f, 1.0f, 1.0f);

		Transition(g_backbuffers[g_frame_index].Get(), backbuffer_state, D3D12_RESOURCE_STATE_PRESENT);

		ThrowIfFailed(g_cmd->Close(), "Close");
		ID3D12CommandList *lists[] = { g_cmd.Get() };
		g_queue->ExecuteCommandLists(1, lists);

		ThrowIfFailed(g_swapchain->Present(1, 0), "Present");

		// Frame pacing: record what this slot's work signals, then wait for the next slot's previous work
		const UINT64 value = g_next_fence_value++;
		ThrowIfFailed(g_queue->Signal(g_fence.Get(), value), "Signal");
		g_fence_values[g_frame_index] = value;

		g_frame_index = g_swapchain->GetCurrentBackBufferIndex();
		if (g_fence->GetCompletedValue() < g_fence_values[g_frame_index])
		{
			ThrowIfFailed(g_fence->SetEventOnCompletion(g_fence_values[g_frame_index], g_fence_event), "SetEventOnCompletion");
			WaitForSingleObject(g_fence_event, INFINITE);
		}
	}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int)
{
	// "-seconds N": quit on its own. Automated runs must never force-kill a process that has a graphics
	// API hooked: killing it mid-frame leaves work queued on the GPU and can upset the display driver.
	double run_seconds = 0.0;
	if (command_line != nullptr)
	{
		if (const wchar_t *arg = wcsstr(command_line, L"-seconds"))
			run_seconds = _wtof(arg + 8);
	}

	WNDCLASSEXW wc = { sizeof(wc) };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_CYGAMECAPTURE));
	wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_CYGAMECAPTURE), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
	wc.lpszClassName = L"CyGameCaptureTestAppD3D12";
	RegisterClassExW(&wc);

	RECT rect = { 0, 0, static_cast<LONG>(kWidth), static_cast<LONG>(kHeight) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"CyGameCapture Test App (D3D12) - scene + HUD", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
	if (hwnd == nullptr)
		return 1;
	ShowWindow(hwnd, SW_SHOW);

	try
	{
		InitD3D(hwnd);
	}
	catch (const std::exception &e)
	{
		MessageBoxA(hwnd, e.what(), "CyGameCaptureTestAppD3D12", MB_ICONERROR);
		return 2;
	}

	LARGE_INTEGER frequency, start, now;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	MSG msg = {};
	while (g_running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (!g_running)
			break;
		QueryPerformanceCounter(&now);
		const double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
		if (run_seconds > 0.0 && elapsed >= run_seconds)
		{
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
			continue;
		}
		try
		{
			RenderFrame(static_cast<float>(elapsed));
		}
		catch (const std::exception &e)
		{
			MessageBoxA(hwnd, e.what(), "CyGameCaptureTestAppD3D12", MB_ICONERROR);
			return 3;
		}
	}

	WaitForGpu();
	if (g_fence_event != nullptr)
		CloseHandle(g_fence_event);
	return 0;
}
