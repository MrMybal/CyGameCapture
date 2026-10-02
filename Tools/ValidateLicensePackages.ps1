# Validates the actual release ZIPs against the source licence texts and checksum manifest.
param([Parameter(Mandatory = $true)][string] $ReleaseDirectory)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'PackageLicences.ps1')
$canonical = Join-Path $root 'LICENCES'
$required = @{
    ReShade = @('SOURCES.txt', 'CyGameCapture-AGPL-3.0.txt', 'CyGameCaptureRS-Notices.md', 'Spout2-LICENSE.txt', 'Spout2-licence.txt', 'ReShade-LICENSE.md', 'DearImGui-MIT.txt')
    OBS = @('SOURCES.txt', 'CyGameCapture-AGPL-3.0.txt', 'CyGameCaptureOBS-Notices.md', 'Spout2-LICENSE.txt', 'Spout2-licence.txt', 'OBS-Studio-COPYING.txt', 'OBS-Studio-COMMITMENT.txt')
    Unreal = @('SOURCES.txt', 'CyGameCaptureUE-MIT.txt', 'CyGameCaptureUE-Notices.md', 'Spout2-LICENSE.txt', 'Spout2-licence.txt')
}
function Assert-Names($entries, $prefix, $names) {
    foreach ($name in $names) {
        if (-not $entries.Contains($prefix + $name)) { throw "Missing licence: $prefix$name" }
    }
}
$archives = @(Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*.zip')
if ($archives.Count -eq 0) { throw 'No release ZIPs found' }
foreach ($file in $archives) {
    $zip = [System.IO.Compression.ZipFile]::OpenRead($file.FullName)
    try {
        $entries = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($entry in $zip.Entries) { [void]$entries.Add($entry.FullName) }
        $top = $zip.Entries[0].FullName.Split('/')[0] + '/'
        if ($file.Name -match 'UnrealPlugin') { Assert-Names $entries ($top + 'LICENCES/') $required.Unreal }
        elseif ($file.Name -match 'ReShade-Addon') { Assert-Names $entries ($top + 'LICENCES/') $required.ReShade }
        elseif ($file.Name -match 'OBS-Plugin') { Assert-Names $entries ($top + 'LICENCES/') $required.OBS }
        else {
            Assert-Names $entries ($top + 'LICENCES/') ($required.ReShade + $required.OBS + $required.Unreal | Select-Object -Unique)
            Assert-Names $entries ($top + 'ReShade-Addon/LICENCES/') $required.ReShade
            Assert-Names $entries ($top + 'OBS-Plugin/LICENCES/') $required.OBS
            foreach ($descriptor in $zip.Entries | Where-Object { $_.FullName.EndsWith('/CyGameCaptureUE.uplugin') }) {
                $plugin = $descriptor.FullName.Substring(0, $descriptor.FullName.LastIndexOf('/') + 1)
                Assert-Names $entries ($plugin + 'LICENCES/') $required.Unreal
            }
        }
        $checked = 0
        foreach ($entry in $zip.Entries | Where-Object { $_.FullName -match '/LICENCES/[^/]+$' }) {
            $name = $entry.FullName.Split('/')[-1]
            $sourceFolder = if ($entry.FullName -match '(^|/)CyGameCaptureUE/LICENCES/') { Join-Path $root 'CyGameCaptureUE/LICENCES' } else { $canonical }
            $source = Join-Path $sourceFolder $name
            if (-not (Test-Path -LiteralPath $source)) { throw "Unknown notice in package: $($entry.FullName)" }
            $stream = $entry.Open()
            $sha = [System.Security.Cryptography.SHA256]::Create()
            try { $hash = [System.BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
            finally { $sha.Dispose(); $stream.Dispose() }
            if ($hash -ne (Get-ProductFileHash -Path $source)) { throw "Modified licence in ZIP: $($entry.FullName)" }
            $checked++
        }
        $unexpected = $zip.Entries | Where-Object { $_.FullName -match '(?i)(\.whl$|bundle\.json$|AssistantRuntime|cytools_core|cyuemcp|\.pdb$|\.log\d*$)' }
        if ($unexpected) { throw "Unexpected SDK/runtime/debug artefact: $($unexpected[0].FullName)" }
        Write-Host "PASS $($file.Name): $checked exact licence copies"
    } finally { $zip.Dispose() }
}
foreach ($line in Get-Content -LiteralPath (Join-Path $ReleaseDirectory 'SHA256SUMS.txt')) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') { throw 'Invalid checksum manifest' }
    $hash, $name = $Matches[1], $Matches[2]
    if ((Get-ProductFileHash -Path (Join-Path $ReleaseDirectory $name)).ToLowerInvariant() -ne $hash) { throw "Archive checksum mismatch: $name" }
}
Write-Host 'PASS archive checksum manifest'
