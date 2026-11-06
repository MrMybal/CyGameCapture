# Copyright (C) 2026 Cyberalien
# SPDX-License-Identifier: AGPL-3.0-or-later
param(
    [string[]] $Versions = @('5.3','5.4','5.5','5.6','5.7','5.8'),
    [string] $SourcePlugin = '',
    [int] $MaxParallelActions = 2
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $SourcePlugin) { $SourcePlugin = Join-Path $root 'CyGameCaptureUE' }
$SourcePlugin = (Resolve-Path -LiteralPath $SourcePlugin).Path
foreach ($version in $Versions) {
    if ($version -notmatch '^5\.[3-8]$') { throw 'Expected a supported packaged UE version, 5.3 to 5.8' }
    $engine = (Get-ItemProperty -LiteralPath "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$version").InstalledDirectory
    $buildCommand = Join-Path $engine 'Engine/Build/BatchFiles/Build.bat'
    if (-not (Test-Path -LiteralPath $buildCommand)) { throw "Unreal $version is not installed" }
    $work = Join-Path $root "build/CyDoc-UE$version"
    $hostRoot = Join-Path $work 'HostProject'
    $hostPlugin = Join-Path $hostRoot 'Plugins/CyGameCaptureUE'
    New-Item -ItemType Directory -Force -Path $hostPlugin | Out-Null
    robocopy $SourcePlugin $hostPlugin /E /XD Intermediate Binaries Saved DerivedDataCache .git /XJ /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw 'Could not prepare the isolated host' }
    $project = Join-Path $hostRoot 'HostProject.uproject'
    [IO.File]::WriteAllText($project,'{"FileVersion":3,"Plugins":[{"Name":"CyGameCaptureUE","Enabled":true}]}')
    Write-Host "Building UnrealEditor $version (source and runtime data are preserved; no engine build)"
    $manifest = Join-Path $hostRoot 'Saved/Manifest-UnrealEditor-Win64-Development.xml'
    & $buildCommand UnrealEditor Win64 Development "-Project=$project" "-plugin=$hostPlugin/CyGameCaptureUE.uplugin" -noubtmakefiles "-manifest=$manifest" "-Log=$work/UnrealBuildTool.log" -nohotreload -WaitMutex "-MaxParallelActions=$MaxParallelActions"
    if ($LASTEXITCODE -ne 0) { throw "Unreal $version help integration did not compile" }
    # The application distributes Editor DLLs and source for game targets. Refresh the
    # existing BuildPlugin-layout inputs with the actual UBT binary, matching source,
    # module manifest and descriptor, preserving engine-version metadata.
    $target = [IO.Path]::GetFullPath((Join-Path $root "build/Plugin_UE$version"))
    if (-not $target.StartsWith([IO.Path]::GetFullPath((Join-Path $root 'build')) + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected package input target' }
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    robocopy $SourcePlugin $target /E /XD Intermediate Binaries Saved DerivedDataCache .git /XJ /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw 'Could not stage the matching plugin source' }
    $binaryTarget = Join-Path $target 'Binaries/Win64'
    New-Item -ItemType Directory -Force -Path $binaryTarget | Out-Null
    foreach ($file in 'UnrealEditor-CyGameCaptureUE.dll','UnrealEditor.modules') {
        Copy-Item -LiteralPath (Join-Path $hostPlugin "Binaries/Win64/$file") -Destination $binaryTarget -Force
    }
    $descriptor = Get-Content -LiteralPath (Join-Path $SourcePlugin 'CyGameCaptureUE.uplugin') -Raw | ConvertFrom-Json
    $engineVersion = Get-Content -LiteralPath (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
    $descriptor.EngineVersion = "$($engineVersion.MajorVersion).$($engineVersion.MinorVersion).$($engineVersion.PatchVersion)"
    $descriptor.Installed = $true
    [IO.File]::WriteAllText((Join-Path $target 'CyGameCaptureUE.uplugin'), ($descriptor | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Write-Host "PASS UnrealEditor ${version}: compiled local help; matching release input $target"
}
exit 0
