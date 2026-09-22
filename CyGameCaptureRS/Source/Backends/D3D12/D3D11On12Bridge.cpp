// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "D3D11On12Bridge.hpp"
#include "Addon/Log.hpp"
#include "ResourceTracker/FormatUtils.hpp"

#include <dxgi1_2.h>
#include <dxgidebug.h>

#include <cstring>
#include <vector>

namespace
{
	/**
	 * When the DXGI debug layer is active (Graphics Tools installed and a debug factory created — ReShade
	 * does this in its own start-up), it calls DebugBreak() on every error it reports. Without a debugger
	 * attached that breakpoint terminates the process. D3D11On12CreateDevice legitimately produces such a
	 * message in a hooked process, so the break is suppressed for the duration of our call only, and the
	 * messages produced meanwhile are copied into the ReShade log.
	 */
	/**
	 * Last resort for the same problem: some debug-layer breakpoints are raised by dxgi.dll itself and are
	 * not covered by IDXGIInfoQueue::SetBreakOnSeverity. A debugger answers an int3 by resuming after it,
	 * which is what this handler does - and only for a breakpoint raised inside dxgi.dll while the bridge
	 * is being created. Everything else is left strictly alone.
	 */
	LONG CALLBACK SkipDxgiDebugBreak(EXCEPTION_POINTERS *info)
	{
		if (info->ExceptionRecord->ExceptionCode != STATUS_BREAKPOINT)
			return EXCEPTION_CONTINUE_SEARCH;

		HMODULE module = nullptr;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				static_cast<LPCSTR>(info->ExceptionRecord->ExceptionAddress), &module))
			return EXCEPTION_CONTINUE_SEARCH;

		char name[MAX_PATH] = {};
		GetModuleFileNameA(module, name, MAX_PATH);
		const size_t length = strlen(name);
		if (length < 8 || _stricmp(name + length - 8, "dxgi.dll") != 0)
			return EXCEPTION_CONTINUE_SEARCH;

		cygc::log::Warning("Skipped a DXGI debug layer breakpoint at 0x%p (Graphics Tools installed, no debugger attached)",
			info->ExceptionRecord->ExceptionAddress);
		info->ContextRecord->Rip += 1;   // int 3 is a single byte: resume just after it, like a debugger would
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	class DxgiDebugBreakScope
	{
	public:
		DxgiDebugBreakScope()
		{
			veh_ = AddVectoredExceptionHandler(1, &SkipDxgiDebugBreak);

			const HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
			if (dxgi == nullptr)
				return;
			using PFN_DXGIGetDebugInterface1 = HRESULT(WINAPI *)(UINT, REFIID, void **);
			const auto get_interface = reinterpret_cast<PFN_DXGIGetDebugInterface1>(GetProcAddress(dxgi, "DXGIGetDebugInterface1"));
			if (get_interface == nullptr)
				return;
			if (FAILED(get_interface(0, __uuidof(IDXGIInfoQueue), reinterpret_cast<void **>(&queue_))) || queue_ == nullptr)
				return;

			first_message_ = queue_->GetNumStoredMessages(DXGI_DEBUG_ALL);
			for (int i = 0; i < 2; ++i)
			{
				const DXGI_INFO_QUEUE_MESSAGE_SEVERITY severity = kSeverities[i];
				previous_[i] = queue_->GetBreakOnSeverity(DXGI_DEBUG_ALL, severity);
				queue_->SetBreakOnSeverity(DXGI_DEBUG_ALL, severity, FALSE);
			}
			cygc::log::Verbose("DXGI debug layer active (break on corruption %d, on error %d) - suppressed during bridge creation",
				static_cast<int>(previous_[0]), static_cast<int>(previous_[1]));
		}

		~DxgiDebugBreakScope()
		{
			if (queue_ == nullptr)
			{
				if (veh_ != nullptr)
					RemoveVectoredExceptionHandler(veh_);
				return;
			}
			const UINT64 count = queue_->GetNumStoredMessages(DXGI_DEBUG_ALL);
			for (UINT64 i = first_message_; i < count; ++i)
			{
				SIZE_T length = 0;
				if (FAILED(queue_->GetMessage(DXGI_DEBUG_ALL, i, nullptr, &length)) || length == 0)
					continue;
				std::vector<char> storage(length);
				auto *const message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE *>(storage.data());
				if (SUCCEEDED(queue_->GetMessage(DXGI_DEBUG_ALL, i, message, &length)))
					cygc::log::Verbose("DXGI debug layer: %.*s", static_cast<int>(message->DescriptionByteLength), message->pDescription);
			}
			for (int i = 0; i < 2; ++i)
				queue_->SetBreakOnSeverity(DXGI_DEBUG_ALL, kSeverities[i], previous_[i]);
			queue_->Release();
			if (veh_ != nullptr)
				RemoveVectoredExceptionHandler(veh_);
		}

		DxgiDebugBreakScope(const DxgiDebugBreakScope &) = delete;
		DxgiDebugBreakScope &operator=(const DxgiDebugBreakScope &) = delete;

	private:
		static constexpr DXGI_INFO_QUEUE_MESSAGE_SEVERITY kSeverities[2] = { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR };
		IDXGIInfoQueue *queue_ = nullptr;
		PVOID veh_ = nullptr;
		UINT64 first_message_ = 0;
		BOOL previous_[2] = { FALSE, FALSE };
	};
}

namespace cygc
{
	D3D11On12Bridge &D3D11On12Bridge::Get()
	{
		static D3D11On12Bridge instance;
		return instance;
	}

	D3D11On12Bridge::~D3D11On12Bridge()
	{
		Shutdown();
	}

	bool D3D11On12Bridge::Initialize(ID3D12Device *device12, ID3D12CommandQueue *queue, std::string &error)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (device11_ != nullptr)
		{
			// Already built. A second device (rare: some games create several) is not supported by one bridge.
			if (device12_ != device12)
			{
				error = "A D3D11On12 bridge already exists for another D3D12 device";
				return false;
			}
			return true;
		}
		if (device12 == nullptr || queue == nullptr)
		{
			error = "No native D3D12 device / command queue available";
			return false;
		}

		log::Verbose("D3D11On12 bridge: creating on device 0x%p / queue 0x%p...", device12, queue);

		IUnknown *queues[] = { queue };
		HRESULT hr;
		{
			const DxgiDebugBreakScope debug_break_scope;
			hr = D3D11On12CreateDevice(device12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0, &device11_, &context11_, nullptr);
		}
		if (FAILED(hr) || device11_ == nullptr)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "D3D11On12CreateDevice failed: 0x%08X", static_cast<unsigned int>(hr));
			error = buffer;
			Shutdown();
			return false;
		}
		log::Verbose("D3D11On12 bridge: device 0x%p / context 0x%p created", device11_, context11_);

		if (FAILED(device11_->QueryInterface(__uuidof(ID3D11On12Device), reinterpret_cast<void **>(&on12_))) || on12_ == nullptr)
		{
			error = "ID3D11On12Device interface not available";
			Shutdown();
			return false;
		}

		device12_ = device12;
		queue_ = queue;
		log::Info("D3D11On12 bridge created on the game's D3D12 device (queue 0x%p)", queue);
		return true;
	}

	void D3D11On12Bridge::ShutdownForDevice(ID3D12Device *device12)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (device12_ == nullptr || device12_ != device12)
			return;
		log::Info("D3D11On12 bridge released with its D3D12 device");
		Shutdown();
	}

	void D3D11On12Bridge::Shutdown()
	{
		if (context11_ != nullptr)
		{
			context11_->Flush();
			context11_->Release();
			context11_ = nullptr;
		}
		if (on12_ != nullptr)
		{
			on12_->Release();
			on12_ = nullptr;
		}
		if (device11_ != nullptr)
		{
			device11_->Release();
			device11_ = nullptr;
		}
		device12_ = nullptr;
		queue_ = nullptr;
	}

	bool D3D11On12Bridge::CreateSharedTexture(uint32_t width, uint32_t height, uint32_t dxgi_format, ID3D11Texture2D **out_texture, HANDLE *out_handle, std::string &error)
	{
		if (device11_ == nullptr || out_texture == nullptr || out_handle == nullptr)
		{
			error = "D3D11On12 bridge not initialized";
			return false;
		}
		*out_texture = nullptr;
		*out_handle = nullptr;

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = static_cast<DXGI_FORMAT>(dxgi_format);
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;   // legacy handle: what Spout receivers open

		ID3D11Texture2D *texture = nullptr;
		HRESULT hr = device11_->CreateTexture2D(&desc, nullptr, &texture);
		if (FAILED(hr) || texture == nullptr)
		{
			char buffer[192];
			snprintf(buffer, sizeof(buffer), "11on12 CreateTexture2D(%ux%u %s) failed: 0x%08X", width, height, FormatName(static_cast<reshade::api::format>(dxgi_format)), static_cast<unsigned int>(hr));
			error = buffer;
			return false;
		}

		IDXGIResource *dxgi_resource = nullptr;
		HANDLE handle = nullptr;
		if (FAILED(texture->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void **>(&dxgi_resource))) || dxgi_resource == nullptr)
		{
			error = "QueryInterface(IDXGIResource) failed on the 11on12 shared texture";
			texture->Release();
			return false;
		}
		hr = dxgi_resource->GetSharedHandle(&handle);
		dxgi_resource->Release();
		if (FAILED(hr) || handle == nullptr)
		{
			error = "GetSharedHandle failed on the 11on12 shared texture";
			texture->Release();
			return false;
		}

		*out_texture = texture;
		*out_handle = handle;
		return true;
	}

	bool D3D11On12Bridge::WrapResource(ID3D12Resource *resource12, D3D12_RESOURCE_STATES state, ID3D11Resource **out_wrapped, std::string &error)
	{
		if (on12_ == nullptr || resource12 == nullptr || out_wrapped == nullptr)
		{
			error = "D3D11On12 bridge not initialized";
			return false;
		}
		*out_wrapped = nullptr;

		D3D11_RESOURCE_FLAGS flags = {};
		const HRESULT hr = on12_->CreateWrappedResource(resource12, &flags, state, state, __uuidof(ID3D11Resource), reinterpret_cast<void **>(out_wrapped));
		if (FAILED(hr) || *out_wrapped == nullptr)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "CreateWrappedResource failed: 0x%08X", static_cast<unsigned int>(hr));
			error = buffer;
			return false;
		}
		return true;
	}

	void D3D11On12Bridge::CopyWrappedToShared(ID3D11Resource *wrapped_source, ID3D11Texture2D *shared_dest)
	{
		if (on12_ == nullptr || context11_ == nullptr || wrapped_source == nullptr || shared_dest == nullptr)
			return;

		ID3D11Resource *resources[] = { wrapped_source };
		on12_->AcquireWrappedResources(resources, 1);
		context11_->CopyResource(shared_dest, wrapped_source);
		on12_->ReleaseWrappedResources(resources, 1);
		// Submits the bridge work to the game's queue, after the copy the game queue already received
		context11_->Flush();
	}
}
