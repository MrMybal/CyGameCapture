# Builds the release archives of CyGameCapture.
#
#   Tools\MakeRelease.cmd [output folder]
#
# Needs a Release build (build.cmd Release) and the Unreal plugin packaged for each engine version by
# RunUAT BuildPlugin into build\Plugin_UE<version> (see CyGameCaptureUE\README.md). Writes
# dist\CyGameCapture-<version>\ :
#
#   CyGameCapture-<version>-Win64\                    the complete package, unpacked, as it is archived
#   CyGameCapture-<version>-Complete-Win64.zip        everything below in one archive
#   CyGameCapture-<version>-ReShade-Addon-Win64.zip   the add-on and the helper of its AI assistant
#   CyGameCapture-<version>-OBS-Plugin-Win64.zip      the OBS plugin and its installer
#   CyGameCapture-<version>-UnrealPlugin-UE<x.y>-Win64.zip   one per engine version, ready for <Project>\Plugins
#   SHA256SUMS.txt, RELEASE_NOTES.md
#
# The script refuses binaries older than their sources, and packages containing symbol files or logs.
param(
    [string] $OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root "bin\Release"
$files = Join-Path $PSScriptRoot "ReleaseFiles"

# The version comes from the one place that defines it
$versionHeader = Get-Content (Join-Path $root "CyGameCaptureCore\Include\CyGameCaptureCore\Version.hpp") -Raw
if ($versionHeader -notmatch '#define CYGC_VERSION_STRING "([^"]+)"') {
    throw "CYGC_VERSION_STRING not found in Version.hpp"
}
$version = $Matches[1]

if ($OutputDirectory -eq "") {
    $OutputDirectory = Join-Path $root ("dist\CyGameCapture-" + $version)
}
$packageName = "CyGameCapture-$version-Win64"
$package = Join-Path $OutputDirectory $packageName

function Section($name) { Write-Host ("  - " + $name) }

function Require($path, $what) {
    if (-not (Test-Path $path)) {
        throw "$what not found: $path"
    }
}

# A binary must be newer than every source file it is built from: a renamed old build is not a release
function Assert-Fresh($binary, [string[]] $sourceFolders) {
    $built = (Get-Item $binary).LastWriteTimeUtc
    foreach ($folder in $sourceFolders) {
        $newer = Get-ChildItem (Join-Path $root $folder) -Recurse -File |
            Where-Object { $_.Extension -in '.cpp', '.hpp', '.h', '.inl', '.rc', '.rc2', '.cs', '.uplugin' -or $_.Name -eq 'CMakeLists.txt' } |
            Where-Object { $_.FullName -notmatch '\\(Intermediate|Binaries)\\' -and $_.LastWriteTimeUtc -gt $built } |
            Select-Object -First 1
        if ($newer) {
            throw "$binary is older than $($newer.FullName): rebuild before making a release"
        }
    }
}

function Assert-Version($binary) {
    $found = (Get-Item $binary).VersionInfo.ProductVersion
    if ($found -ne $version) {
        throw "$binary is version $found, Version.hpp says $version"
    }
}

function New-Zip($sourceFolder, $zipPath, $topFolder) {
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    $base = (Resolve-Path $sourceFolder).Path.TrimEnd('\') + '\'
    $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        Get-ChildItem $sourceFolder -Recurse -File | Sort-Object FullName | ForEach-Object {
            # Forward slashes: the separator the zip format defines, read correctly everywhere
            $entry = $topFolder + "/" + $_.FullName.Substring($base.Length).Replace('\', '/')
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $entry,
                [System.IO.Compression.CompressionLevel]::Optimal)
        }
    } finally {
        $zip.Dispose()
    }
}

function New-Folder($path) {
    New-Item -ItemType Directory -Force $path | Out-Null
    return $path
}

# ----------------------------------------------------------------- inputs
$addonBinary = Join-Path $bin "CyGameCaptureRS.addon64"
$aiBinary = Join-Path $bin "CyGameCaptureAI.exe"
$obsBinary = Join-Path $bin "obs-plugins\64bit\CyGameCaptureOBS.dll"
$obsData = Join-Path $bin "data\obs-plugins\CyGameCaptureOBS"
Require $addonBinary "The ReShade add-on (build.cmd Release)"
Require $aiBinary "The AI helper (build.cmd Release)"
Require $obsBinary "The OBS plugin (Tools\GenerateObsImportLib.cmd, then build.cmd Release)"
Require $obsData "The OBS plugin data"

foreach ($binary in $addonBinary, $aiBinary, $obsBinary) { Assert-Version $binary }
Assert-Fresh $addonBinary @("CyGameCaptureRS\Source", "CyGameCaptureCore", "Resources\Branding")
Assert-Fresh $aiBinary @("CyGameCaptureAI", "CyGameCaptureCore", "Resources\Branding")
Assert-Fresh $obsBinary @("CyGameCaptureOBS\Source", "CyGameCaptureCore", "Resources\Branding")

$unrealPackages = @(Get-ChildItem (Join-Path $root "build") -Directory -Filter "Plugin_UE*" -ErrorAction SilentlyContinue |
    Sort-Object { [version]($_.Name -replace '^Plugin_UE', '') })
if ($unrealPackages.Count -eq 0) {
    throw "No Unreal plugin package in build\Plugin_UE*: run RunUAT BuildPlugin first (CyGameCaptureUE\README.md)"
}
foreach ($unrealPackage in $unrealPackages) {
    $dll = Join-Path $unrealPackage.FullName "Binaries\Win64\UnrealEditor-CyGameCaptureUE.dll"
    Require $dll "The Unreal plugin binary"
    Assert-Fresh $dll @("CyGameCaptureUE\Source", "CyGameCaptureUE\CyGameCaptureUE.uplugin")
    $descriptor = Get-Content (Join-Path $unrealPackage.FullName "CyGameCaptureUE.uplugin") -Raw | ConvertFrom-Json
    if ($descriptor.VersionName -ne $version) {
        throw "$($unrealPackage.Name) is version $($descriptor.VersionName), Version.hpp says $version"
    }
}

Write-Host ""
Write-Host "  CyGameCapture $version release"
Write-Host "  to $OutputDirectory"
Write-Host ""

if (Test-Path $OutputDirectory) {
    Remove-Item $OutputDirectory -Recurse -Force
}
New-Folder $package | Out-Null

$licence = Join-Path $root "LICENSE"
$rsThirdParty = Join-Path $root "CyGameCaptureRS\ThirdParty"

# ----------------------------------------------------------------- the ReShade add-on
Section "ReShade add-on"
$addon = New-Folder (Join-Path $package "ReShade-Addon")
Copy-Item $addonBinary $addon
# The AI assistant's helper; the add-on starts it from its own folder
Copy-Item $aiBinary $addon
Copy-Item (Join-Path $files "ReShade-Addon\README.txt") $addon
Copy-Item (Join-Path $PSScriptRoot "PackageFiles\LISEZMOI-installer-dans-un-jeu.txt") (Join-Path $addon "LISEZMOI.txt")
Copy-Item $licence (Join-Path $addon "LICENSE.txt")
$notices = New-Folder (Join-Path $addon "Licences")
Copy-Item (Join-Path $rsThirdParty "THIRD_PARTY_NOTICES.md") $notices
Copy-Item (Join-Path $rsThirdParty "reshade\LICENSE.md") (Join-Path $notices "ReShade-LICENSE.md")
Copy-Item (Join-Path $rsThirdParty "imgui\LICENSE.txt") (Join-Path $notices "DearImGui-LICENSE.txt")
Copy-Item (Join-Path $rsThirdParty "Spout2\LICENSE") (Join-Path $notices "Spout2-LICENSE.txt")
Copy-Item (Join-Path $rsThirdParty "Spout2\licence.txt") (Join-Path $notices "Spout2-licence.txt")

# ----------------------------------------------------------------- the OBS plugin
Section "OBS plugin"
$obs = New-Folder (Join-Path $package "OBS-Plugin")
$obsPlugin = New-Folder (Join-Path $obs "CyGameCaptureOBS\bin\64bit")
Copy-Item $obsBinary $obsPlugin
Copy-Item $obsData (Join-Path $obs "CyGameCaptureOBS\data") -Recurse
foreach ($name in "README.txt", "LISEZMOI.txt", "Install-OBS-Plugin.cmd", "Uninstall-OBS-Plugin.cmd") {
    Copy-Item (Join-Path $files "OBS-Plugin\$name") $obs
}
Copy-Item $licence (Join-Path $obs "LICENSE.txt")
$notices = New-Folder (Join-Path $obs "Licences")
$obsThirdParty = Join-Path $root "CyGameCaptureOBS\ThirdParty"
Copy-Item (Join-Path $obsThirdParty "THIRD_PARTY_NOTICES.md") $notices
Copy-Item (Join-Path $obsThirdParty "obs-studio\COPYING") (Join-Path $notices "OBS-Studio-COPYING.txt")
Copy-Item (Join-Path $obsThirdParty "obs-studio\COMMITMENT") (Join-Path $notices "OBS-Studio-COMMITMENT.txt")
Copy-Item (Join-Path $rsThirdParty "Spout2\LICENSE") (Join-Path $notices "Spout2-LICENSE.txt")
Copy-Item (Join-Path $rsThirdParty "Spout2\licence.txt") (Join-Path $notices "Spout2-licence.txt")

# ----------------------------------------------------------------- the Unreal plugin
Section "Unreal plugin"
$unreal = New-Folder (Join-Path $package "Unreal-Plugin")
$unrealSource = Join-Path $root "CyGameCaptureUE"
foreach ($unrealPackage in $unrealPackages) {
    $engine = $unrealPackage.Name -replace '^Plugin_UE', ''
    $target = New-Folder (Join-Path $unreal ("UE" + $engine + "\CyGameCaptureUE"))
    # Intermediate holds the build's leftovers, symbol files carry build paths: neither is for installing.
    # The project compiles the plugin from Source for its game targets.
    robocopy $unrealPackage.FullName $target /E /XD Intermediate /XF *.pdb /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copy of $($unrealPackage.FullName) failed" }
    # The documentation and the licence as they are now, not as they were when that engine version was built
    Copy-Item (Join-Path $unrealSource "README.md") $target -Force
    Copy-Item (Join-Path $unrealSource "LICENSE") $target -Force
    robocopy (Join-Path $unrealSource "Docs") (Join-Path $target "Docs") /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copy of the Unreal documentation failed" }
    Write-Host ("      Unreal " + $engine)
}

# ----------------------------------------------------------------- documentation and top level
Section "documentation"
$docs = New-Folder (Join-Path $package "Documentation")
Copy-Item (Join-Path $root "README.md") (Join-Path $docs "CyGameCapture - README.md")
Copy-Item (Join-Path $root "CHANGELOG.md") $docs
Copy-Item (Join-Path $root "Docs\CyGameCapture_Architecture.md") $docs
Copy-Item (Join-Path $root "CyGameCaptureOBS\README.md") (Join-Path $docs "CyGameCaptureOBS - README.md")
Copy-Item (Join-Path $root "CyGameCaptureUE\README.md") (Join-Path $docs "CyGameCaptureUE - README.md")
# The README shows the logo through a relative path, which has to resolve from here as well
$branding = Join-Path $root "Resources\Branding"
New-Folder (Join-Path $docs "Resources\Branding") | Out-Null
Copy-Item (Join-Path $branding "CyGameCapture_Logo_256.png") (Join-Path $docs "Resources\Branding")

Copy-Item $licence (Join-Path $package "LICENSE.txt")
(Get-Content (Join-Path $files "README.txt") -Raw -Encoding UTF8).Replace("@VERSION@", $version) |
    Set-Content (Join-Path $package "README.txt") -Encoding UTF8 -NoNewline
Copy-Item (Join-Path $branding "CyGameCapture.ico") $package

# ----------------------------------------------------------------- what must never be in it
$unwanted = Get-ChildItem $package -Recurse -File |
    Where-Object { $_.Name -match '\.(pdb|ilk|exp|log\d*|obj|lib|tmp|bak)$' -or $_.Name -in 'ReShade.ini', 'ReShade.log' }
if ($unwanted) {
    throw ("Files that do not belong in a release:`n" + (($unwanted | ForEach-Object { $_.FullName }) -join "`n"))
}

# ----------------------------------------------------------------- archives
Section "archives"
$archives = @()
$zip = Join-Path $OutputDirectory "CyGameCapture-$version-Complete-Win64.zip"
New-Zip $package $zip $packageName
$archives += $zip

$zip = Join-Path $OutputDirectory "CyGameCapture-$version-ReShade-Addon-Win64.zip"
New-Zip $addon $zip "CyGameCapture-$version-ReShade-Addon"
$archives += $zip

$zip = Join-Path $OutputDirectory "CyGameCapture-$version-OBS-Plugin-Win64.zip"
New-Zip $obs $zip "CyGameCapture-$version-OBS-Plugin"
$archives += $zip

foreach ($unrealPackage in $unrealPackages) {
    $engine = $unrealPackage.Name -replace '^Plugin_UE', ''
    # Extracted into <Project>\Plugins, the archive gives <Project>\Plugins\CyGameCaptureUE directly
    $zip = Join-Path $OutputDirectory "CyGameCapture-$version-UnrealPlugin-UE$engine-Win64.zip"
    New-Zip (Join-Path $unreal ("UE" + $engine + "\CyGameCaptureUE")) $zip "CyGameCaptureUE"
    $archives += $zip
}

$sums = foreach ($archive in $archives) {
    $hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $(Split-Path $archive -Leaf)"
}
# LF line ends and no BOM, the format sha256sum -c reads
[System.IO.File]::WriteAllText((Join-Path $OutputDirectory "SHA256SUMS.txt"), (($sums -join "`n") + "`n"))

# The notes of this version, taken from the changelog
$changelog = Get-Content (Join-Path $root "CHANGELOG.md") -Raw -Encoding UTF8
$pattern = "(?ms)^## " + [regex]::Escape($version) + "\b.*?(?=^## |\z)"
$notes = [regex]::Match($changelog, $pattern)
if (-not $notes.Success) { throw "No section for $version in CHANGELOG.md" }
[System.IO.File]::WriteAllText((Join-Path $OutputDirectory "RELEASE_NOTES.md"), $notes.Value.TrimEnd() + "`n")

Write-Host ""
foreach ($archive in $archives) {
    Write-Host ("  {0,-58} {1,8:N1} MB" -f (Split-Path $archive -Leaf), ((Get-Item $archive).Length / 1MB))
}
Write-Host ""
Write-Host "  Done: $OutputDirectory"
