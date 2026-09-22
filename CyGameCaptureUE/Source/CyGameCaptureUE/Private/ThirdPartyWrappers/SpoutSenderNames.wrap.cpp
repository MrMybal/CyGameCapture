// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// Compiles the Spout2 SDK source SpoutSenderNames.cpp inside this module with the engine's Windows guards.
#include "Spout/CySpoutIncludes.h"
#if CYGC_WITH_SPOUT
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
THIRD_PARTY_INCLUDES_START
#pragma warning(push, 1)
#pragma warning(disable: 4668 4100 4189 4244 4245 4267 4310 4324 4389 4456 4457 4458 4459 4505 4701 4702 4706 4838 4996 5038 5054 6011 6031 6255 6262 6386 6387)
// The engine's PostWindowsApi.h undefines a few Windows API macros the Spout sources rely on
#ifndef SendMessage
#define SendMessage SendMessageW
#endif
#ifndef PostMessage
#define PostMessage PostMessageW
#endif
#ifndef GetMessage
#define GetMessage GetMessageW
#endif
#ifndef MessageBox
#define MessageBox MessageBoxW
#endif
#ifndef CreateWindow
#define CreateWindow CreateWindowW
#endif
#ifndef GetObject
#define GetObject GetObjectW
#endif
#include "../../../ThirdParty/Spout2/SpoutGL/SpoutSenderNames.cpp"
#undef SendMessage
#undef PostMessage
#undef GetMessage
#undef MessageBox
#undef CreateWindow
#undef GetObject
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif
