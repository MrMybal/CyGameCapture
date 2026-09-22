// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureTestApp — a tiny D3D11 "game" used to validate CyGameCaptureRS end to end.
//
// Frame structure (deliberately similar to a real engine):
//   1. Scene (HDR)   : animated shapes rendered into an R16G16B16A16_FLOAT render target with depth
//   2. Bloom-ish     : half resolution R11G11B10_FLOAT target (an unsupported-format candidate)
//   3. Tonemap       : fullscreen pass HDR -> R8G8B8A8_UNORM "clean scene" render target
//   4. Composite     : clean scene copied onto the back buffer
//   5. HUD           : coloured rectangles + moving bars drawn on top of the back buffer only
//
// The player sees 4+5, CyGameCaptureRS should be able to pick 3 (clean, no HUD).
#include <Windows.h>
#include <CyGameCaptureBranding.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace
{
	constexpr UINT kWidth = 1280;
	constexpr UINT kHeight = 720;

	const char *const kShaderSource = R"HLSL(
cbuffer Constants : register(b0)
{
	float4 params;   // x = time, y = aspect, z = mode, w = unused
	float4 color;
};
Texture2D    tex0 : register(t0);
SamplerState samp : register(s0);

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

// Fullscreen triangle
VSOut VSFullscreen(uint id : SV_VertexID)
{
	VSOut o;
	float2 uv = float2((id << 1) & 2, id & 2);
	o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
	o.uv = uv;
	return o;
}

// Quad from constants: color.xy = center, color.zw = half size (in NDC)
VSOut VSQuad(uint id : SV_VertexID)
{
	VSOut o;
	float2 corners[6] = { float2(-1,-1), float2(-1,1), float2(1,-1), float2(1,-1), float2(-1,1), float2(1,1) };
	float2 c = corners[id];
	o.pos = float4(color.xy + c * color.zw, 0, 1);
	o.uv = c * 0.5 + 0.5;
	return o;
}

// Animated HDR scene: moving blobs + rotating bars + gradient.
// It also writes SV_Depth so the depth buffer holds something meaningful: the blobs sit close to the
// camera and the background far away, which is what a depth-based capture needs to be tested against.
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

	// Depth: the nearest blob wins, everything else is the far plane. Standard (non reversed) Z, so 0 is
	// the near plane and 1 the far plane, like a plain Direct3D projection.
	float nearest = 1.0;
	for (int m = 0; m < 5; ++m)
	{
		float fm = m;
		float2 c = 0.35 * float2(cos(t * 0.7 + fm * 1.3), sin(t * 0.9 + fm * 2.1));
		float d = length(p - c);
		float blob = smoothstep(0.18, 0.0, d);            // 1 inside the blob, 0 outside
		nearest = min(nearest, lerp(1.0, 0.05 + 0.15 * fm, blob));
	}
	// A gentle vertical gradient for the background, so the far field is not a flat value either
	out_depth = min(nearest, 0.55 + 0.45 * i.uv.y);

	return float4(col, 1);
}

// "Bloom": blurred bright pass at half resolution
float4 PSBloom(VSOut i) : SV_Target
{
	float3 acc = 0;
	for (int y = -2; y <= 2; ++y)
		for (int x = -2; x <= 2; ++x)
			acc += max(tex0.Sample(samp, i.uv + float2(x, y) * 0.004).rgb - 1.0, 0.0);
	return float4(acc / 25.0, 1);
}

// Tonemap HDR + bloom -> clean LDR scene
// `color.xy` scales the texture coordinates so a pass reads only the part of its source that the
// previous pass actually rendered into. This is what dynamic resolution looks like from the inside:
// the texture keeps its full size and only a corner of it holds the current frame.
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

// HUD element: flat colour with a thin border
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

	struct RenderTarget
	{
		ComPtr<ID3D11Texture2D> texture;
		ComPtr<ID3D11RenderTargetView> rtv;
		ComPtr<ID3D11ShaderResourceView> srv;
		UINT width = 0, height = 0;
	};

	ComPtr<ID3D11Device> g_device;
	ComPtr<ID3D11DeviceContext> g_context;
	ComPtr<IDXGISwapChain> g_swapchain;
	ComPtr<ID3D11RenderTargetView> g_backbuffer_rtv;
	ComPtr<ID3D11VertexShader> g_vs_fullscreen, g_vs_quad;
	ComPtr<ID3D11PixelShader> g_ps_scene, g_ps_bloom, g_ps_tonemap, g_ps_copy, g_ps_hud;
	ComPtr<ID3D11Buffer> g_constants;
	ComPtr<ID3D11SamplerState> g_sampler;
	ComPtr<ID3D11DepthStencilView> g_dsv;
	ComPtr<ID3D11DepthStencilState> g_ds_state;
	RenderTarget g_scene_hdr, g_bloom, g_scene_clean;
	bool g_running = true;
	float g_time = 0.0f;              // shared with the pass helper, which fills the constants itself

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

	bool CompileShader(const char *entry, const char *target, ID3DBlob **blob)
	{
		ComPtr<ID3DBlob> errors;
		const HRESULT hr = D3DCompile(kShaderSource, strlen(kShaderSource), "test.hlsl", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob, &errors);
		if (FAILED(hr))
		{
			MessageBoxA(nullptr, errors ? static_cast<const char *>(errors->GetBufferPointer()) : "unknown error", "Shader compilation failed", MB_ICONERROR);
			return false;
		}
		return true;
	}

	bool CreateRenderTarget(RenderTarget &rt, UINT width, UINT height, DXGI_FORMAT format, const char *debug_name)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &rt.texture)))
			return false;
		if (FAILED(g_device->CreateRenderTargetView(rt.texture.Get(), nullptr, &rt.rtv)))
			return false;
		if (FAILED(g_device->CreateShaderResourceView(rt.texture.Get(), nullptr, &rt.srv)))
			return false;
		rt.texture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen(debug_name)), debug_name);
		rt.width = width;
		rt.height = height;
		return true;
	}

	bool InitD3D(HWND hwnd)
	{
		DXGI_SWAP_CHAIN_DESC scd = {};
		scd.BufferCount = 2;
		scd.BufferDesc.Width = kWidth;
		scd.BufferDesc.Height = kHeight;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = hwnd;
		scd.SampleDesc.Count = 1;
		scd.Windowed = TRUE;
		scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &scd, &g_swapchain, &g_device, nullptr, &g_context)))
			return false;

		ComPtr<ID3D11Texture2D> backbuffer;
		g_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
		if (FAILED(g_device->CreateRenderTargetView(backbuffer.Get(), nullptr, &g_backbuffer_rtv)))
			return false;

		ComPtr<ID3DBlob> blob;
		if (!CompileShader("VSFullscreen", "vs_5_0", &blob) || FAILED(g_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_vs_fullscreen))) return false;
		if (!CompileShader("VSQuad", "vs_5_0", &blob) || FAILED(g_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_vs_quad))) return false;
		if (!CompileShader("PSScene", "ps_5_0", &blob) || FAILED(g_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps_scene))) return false;
		if (!CompileShader("PSBloom", "ps_5_0", &blob) || FAILED(g_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps_bloom))) return false;
		if (!CompileShader("PSTonemap", "ps_5_0", &blob) || FAILED(g_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps_tonemap))) return false;
		if (!CompileShader("PSCopy", "ps_5_0", &blob) || FAILED(g_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps_copy))) return false;
		if (!CompileShader("PSHud", "ps_5_0", &blob) || FAILED(g_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps_hud))) return false;

		D3D11_BUFFER_DESC cbd = {};
		cbd.ByteWidth = sizeof(Constants);
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(g_device->CreateBuffer(&cbd, nullptr, &g_constants)))
			return false;

		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		if (FAILED(g_device->CreateSamplerState(&sd, &g_sampler)))
			return false;

		// Depth buffer for the scene pass (gives the inspector a depth candidate)
		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = kWidth;
		dd.Height = kHeight;
		dd.MipLevels = 1;
		dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		ComPtr<ID3D11Texture2D> depth;
		if (FAILED(g_device->CreateTexture2D(&dd, nullptr, &depth)) || FAILED(g_device->CreateDepthStencilView(depth.Get(), nullptr, &g_dsv)))
			return false;
		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable = TRUE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(g_device->CreateDepthStencilState(&dsd, &g_ds_state)))
			return false;

		if (!CreateRenderTarget(g_scene_hdr, kWidth, kHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, "SceneColorHDR")) return false;
		if (!CreateRenderTarget(g_bloom, kWidth / 2, kHeight / 2, DXGI_FORMAT_R11G11B10_FLOAT, "BloomHalfRes")) return false;
		if (!CreateRenderTarget(g_scene_clean, kWidth, kHeight, DXGI_FORMAT_R8G8B8A8_UNORM, "SceneCleanLDR")) return false;
		return true;
	}

	void SetConstants(const Constants &c)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(g_context->Map(g_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &c, sizeof(c));
			g_context->Unmap(g_constants.Get(), 0);
		}
	}

	void SetViewport(UINT width, UINT height)
	{
		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(width);
		vp.Height = static_cast<float>(height);
		vp.MaxDepth = 1.0f;
		g_context->RSSetViewports(1, &vp);
	}

	// `source_scale` is the fraction of its source texture the pass has to read, which is the scale the
	// previous pass rendered at.
	void FullscreenPass(ID3D11RenderTargetView *rtv, UINT width, UINT height, ID3D11PixelShader *ps, ID3D11ShaderResourceView *input,
		ID3D11DepthStencilView *dsv = nullptr, float source_scale = 1.0f)
	{
		Constants c = {};
		c.params[0] = g_time;
		c.params[1] = static_cast<float>(kWidth) / static_cast<float>(kHeight);
		c.color[0] = source_scale;
		c.color[1] = source_scale;
		SetConstants(c);

		ID3D11ShaderResourceView *null_srv = nullptr;
		g_context->PSSetShaderResources(0, 1, &null_srv);
		g_context->OMSetRenderTargets(1, &rtv, dsv);
		SetViewport(width, height);
		g_context->VSSetShader(g_vs_fullscreen.Get(), nullptr, 0);
		g_context->PSSetShader(ps, nullptr, 0);
		g_context->PSSetShaderResources(0, 1, &input);
		g_context->Draw(3, 0);
	}

	void HudQuad(float cx, float cy, float hw, float hh, float r, float g, float b)
	{
		Constants c = {};
		c.params[0] = r; c.params[1] = g; c.params[2] = b;
		c.color[0] = cx; c.color[1] = cy; c.color[2] = hw; c.color[3] = hh;
		SetConstants(c);
		g_context->Draw(6, 0);
	}

	/**
	* Dynamic resolution, the way engines actually do it: the render targets keep their full size and
	* the scene is drawn into a sub-rectangle of them, then upscaled on the way to the back buffer.
	* Nothing is ever reallocated, so the resource descriptions never change - only the viewport does.
	*/
	float ResolutionScale(float time)
	{
		return 0.75f + 0.25f * sinf(time * 0.6f);   // between 50 % and 100 % of the pixels
	}

	void RenderFrame(float time)
	{
		g_time = time;
		const float scale = ResolutionScale(time);
		const UINT scene_width = static_cast<UINT>(kWidth * scale);
		const UINT scene_height = static_cast<UINT>(kHeight * scale);
		const float source_scale = static_cast<float>(scene_width) / static_cast<float>(kWidth);

		ID3D11Buffer *cb = g_constants.Get();
		g_context->VSSetConstantBuffers(0, 1, &cb);
		g_context->PSSetConstantBuffers(0, 1, &cb);
		ID3D11SamplerState *sampler = g_sampler.Get();
		g_context->PSSetSamplers(0, 1, &sampler);
		g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		g_context->IASetInputLayout(nullptr);

		// 1. Scene (HDR) with depth. FullscreenPass sets the constants of each pass itself, including
		//    the fraction of its source it has to read.
		const float clear_hdr[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		g_context->ClearRenderTargetView(g_scene_hdr.rtv.Get(), clear_hdr);
		g_context->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		g_context->OMSetDepthStencilState(g_ds_state.Get(), 0);
		FullscreenPass(g_scene_hdr.rtv.Get(), scene_width, scene_height, g_ps_scene.Get(), nullptr, g_dsv.Get());
		g_context->OMSetDepthStencilState(nullptr, 0);

		// 2. Bloom (half resolution, R11G11B10), at the same scale as the scene
		FullscreenPass(g_bloom.rtv.Get(), g_bloom.width * scene_width / kWidth, g_bloom.height * scene_height / kHeight,
			g_ps_bloom.Get(), g_scene_hdr.srv.Get(), nullptr, source_scale);

		// 3. Tonemap -> clean LDR scene (this is the buffer CyGameCaptureRS should pick).
		//    Still rendered at the scaled resolution: this is the buffer whose active area shrinks.
		FullscreenPass(g_scene_clean.rtv.Get(), scene_width, scene_height, g_ps_tonemap.Get(), g_scene_hdr.srv.Get(),
			nullptr, source_scale);

		// 4. Upscale the clean scene onto the full-size back buffer, exactly as a game does at the end
		//    of a dynamically scaled frame.
		FullscreenPass(g_backbuffer_rtv.Get(), kWidth, kHeight, g_ps_copy.Get(), g_scene_clean.srv.Get(),
			nullptr, source_scale);

		// 5. HUD on top of the back buffer only
		ID3D11ShaderResourceView *null_srv = nullptr;
		g_context->PSSetShaderResources(0, 1, &null_srv);
		g_context->VSSetShader(g_vs_quad.Get(), nullptr, 0);
		g_context->PSSetShader(g_ps_hud.Get(), nullptr, 0);
		HudQuad(-0.72f, 0.82f, 0.25f, 0.06f, 0.9f, 0.2f, 0.2f);                                  // health bar background
		HudQuad(-0.72f - 0.25f * (1.0f - (0.5f + 0.5f * sinf(time))), 0.82f, 0.25f * (0.5f + 0.5f * sinf(time)), 0.045f, 0.2f, 0.9f, 0.2f); // animated health
		HudQuad(0.75f, 0.80f, 0.18f, 0.16f, 0.1f, 0.3f, 0.8f);                                   // minimap
		HudQuad(0.0f, -0.88f, 0.55f, 0.05f, 0.85f, 0.75f, 0.1f);                                 // ammo / hotbar
		HudQuad(0.0f, 0.0f, 0.012f, 0.012f, 1.0f, 1.0f, 1.0f);                                   // crosshair

		g_swapchain->Present(1, 0);
	}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
	WNDCLASSEXW wc = { sizeof(wc) };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_CYGAMECAPTURE));
	wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_CYGAMECAPTURE), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
	wc.lpszClassName = L"CyGameCaptureTestApp";
	RegisterClassExW(&wc);

	RECT rect = { 0, 0, static_cast<LONG>(kWidth), static_cast<LONG>(kHeight) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"CyGameCapture Test App (D3D11) - scene + HUD", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
	if (hwnd == nullptr)
		return 1;
	ShowWindow(hwnd, SW_SHOW);

	if (!InitD3D(hwnd))
	{
		MessageBoxW(hwnd, L"Direct3D 11 initialization failed", L"CyGameCaptureTestApp", MB_ICONERROR);
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
		RenderFrame(static_cast<float>(static_cast<double>(now.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart)));
	}
	return 0;
}
