// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureUEModule.h"
#include "CyGameCaptureLog.h"
#include "CyGameCaptureSettings.h"

DEFINE_LOG_CATEGORY(LogCyGameCapture);

bool CyGameCaptureDebugLoggingEnabled()
{
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	return Settings != nullptr && Settings->bDebugLogging;
}

void FCyGameCaptureUEModule::StartupModule()
{
#if CYGC_WITH_SPOUT
	const TCHAR* SpoutState = TEXT("yes");
#else
	const TCHAR* SpoutState = TEXT("no");
#endif
	CYGC_LOG(Log, TEXT("Module started (Spout support compiled: %s)"), SpoutState);
}

void FCyGameCaptureUEModule::ShutdownModule()
{
	CYGC_LOG(Log, TEXT("Module shut down"));
}

IMPLEMENT_MODULE(FCyGameCaptureUEModule, CyGameCaptureUE)
