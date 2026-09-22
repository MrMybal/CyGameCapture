// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCyGameCapture, Log, All);

// Verbose logging gated by the project setting (checked once per call, cheap)
bool CyGameCaptureDebugLoggingEnabled();

#define CYGC_LOG(Verbosity, Format, ...) UE_LOG(LogCyGameCapture, Verbosity, TEXT("[CyGameCaptureUE] ") Format, ##__VA_ARGS__)
#define CYGC_DEBUG(Format, ...) do { if (CyGameCaptureDebugLoggingEnabled()) { UE_LOG(LogCyGameCapture, Log, TEXT("[CyGameCaptureUE][debug] ") Format, ##__VA_ARGS__); } } while (0)
