// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — module interface.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCyGameCaptureUEModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FCyGameCaptureUEModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FCyGameCaptureUEModule>("CyGameCaptureUE");
	}
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("CyGameCaptureUE");
	}
};
