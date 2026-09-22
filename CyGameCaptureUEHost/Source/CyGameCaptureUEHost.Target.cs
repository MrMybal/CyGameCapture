// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

using UnrealBuildTool;
using System.Collections.Generic;

public class CyGameCaptureUEHostTarget : TargetRules
{
	public CyGameCaptureUEHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CyGameCaptureUEHost");
	}
}
