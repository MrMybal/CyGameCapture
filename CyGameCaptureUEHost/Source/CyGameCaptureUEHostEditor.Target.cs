// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

using UnrealBuildTool;
using System.Collections.Generic;

public class CyGameCaptureUEHostEditorTarget : TargetRules
{
	public CyGameCaptureUEHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("CyGameCaptureUEHost");
	}
}
