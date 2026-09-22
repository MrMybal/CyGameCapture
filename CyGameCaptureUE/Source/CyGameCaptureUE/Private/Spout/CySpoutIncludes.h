// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — single place where Windows / Direct3D / Spout2 headers enter the module.
// Everything is wrapped with the engine platform-type guards so the rest of the code stays
// clean of Windows macros.
#pragma once

#include "CoreMinimal.h"

#if CYGC_WITH_SPOUT

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable: 4668 4100 4189 4244 4245 4267 4310 4324 4389 4456 4457 4458 4459 4505 4701 4702 4706 4838 4996 5038 5054 6011 6031 6255 6262 6386 6387)
#include <mmsystem.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include "SpoutCommon.h"
#include "SpoutUtils.h"
#include "SpoutSharedMemory.h"
#include "SpoutSenderNames.h"
#include "SpoutFrameCount.h"
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END
#include "Windows/PostWindowsApi.h"      // undoes the Windows API macros (GetNextSibling, GetObject, SendMessage, ...) the SDK headers define
#include "Windows/HideWindowsPlatformTypes.h"

template <typename T>
using TCyComPtr = Microsoft::WRL::ComPtr<T>;

#endif // CYGC_WITH_SPOUT
