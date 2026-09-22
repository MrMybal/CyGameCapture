// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — multi-stream Spout2 sender/receiver for Unreal Engine.
// Single runtime module: the Spout2 SDK subset is compiled straight into it through the
// wrapper files in Private/ThirdPartyWrappers (no separate DLL, no prebuilt binaries,
// no absolute paths). Supported: Win64, UE 4.27 .. 5.8, D3D11 and D3D12 RHIs.
using System.IO;
using UnrealBuildTool;

public class CyGameCaptureUE : ModuleRules
{
	public CyGameCaptureUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;          // the Spout2 SDK uses try/catch in a few places
		bUseUnity = false;                 // third-party wrappers must not be merged with engine-facing code

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RHI",
			"RenderCore",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"Slate",
			"SlateCore",
			"InputCore",
		});

		string ThirdPartyDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "Spout2"));
		PrivateIncludePaths.Add(Path.Combine(ThirdPartyDir, "SpoutGL"));
		PrivateIncludePaths.Add(Path.Combine(ThirdPartyDir, "SpoutDX"));

		bool bIsUE5_1OrLater = Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 1);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDefinitions.Add("CYGC_WITH_SPOUT=1");

			// ID3D12DynamicRHI (public since 5.1) gives RHISignalManualFence, needed to order the D3D11On12
			// bridge after the engine's own D3D12 submissions. Older engines use SubmitCommandsHint instead.
			if (bIsUE5_1OrLater)
			{
				PrivateDependencyModuleNames.Add("D3D12RHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");   // d3dx12.h pulled by D3D12ThirdParty.h
				PrivateDefinitions.Add("CYGC_HAS_ID3D12DYNAMICRHI=1");
			}
			else
			{
				PrivateDefinitions.Add("CYGC_HAS_ID3D12DYNAMICRHI=0");
			}

			PublicSystemLibraries.AddRange(new string[]
			{
				"d3d11.lib",
				"d3d12.lib",
				"dxgi.lib",
				"dxguid.lib",
				"winmm.lib",     // Spout timer / frame count helpers
				"shell32.lib",
				"advapi32.lib",  // Spout registry settings
				"version.lib",
				"comctl32.lib",
				"psapi.lib",
			});
		}
		else
		{
			PrivateDefinitions.Add("CYGC_WITH_SPOUT=0");
			PrivateDefinitions.Add("CYGC_HAS_ID3D12DYNAMICRHI=0");
		}
	}
}
