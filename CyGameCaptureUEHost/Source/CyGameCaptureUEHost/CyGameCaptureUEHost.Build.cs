// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

using UnrealBuildTool;

public class CyGameCaptureUEHost : ModuleRules
{
	public CyGameCaptureUEHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "CyGameCaptureUE" });
	}
}
