# Assembles the ready-to-run test folder from a Release build.
#
#   Tools\MakeTestPackage.cmd [dossier de sortie] [noue]
#
# Everything it copies has to exist already: run build.cmd Release first. The
# Unreal plugin packages are optional and come from build\Plugin_UE*.
param(
    [string] $OutputDirectory = "",
    [switch] $NoUnreal,
    # Symbol files (.pdb) carry the paths of the machine they were built on, the user folder included
    # for the Unreal ones: useful to read a crash here, not something to hand out. Off by default.
    [switch] $WithSymbols
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root "bin\Release"
$files = Join-Path $PSScriptRoot "PackageFiles"

if ($OutputDirectory -eq "") {
    $OutputDirectory = Join-Path $root "dist\CyGameCapture_Test"
}

function Require($path, $what) {
    if (-not (Test-Path $path)) {
        throw "$what introuvable : $path`nLancez d'abord build.cmd Release."
    }
}

Require (Join-Path $bin "CyGameCaptureRS.addon64") "l'add-on ReShade"
Require (Join-Path $bin "obs-plugins\64bit\CyGameCaptureOBS.dll") "le plugin OBS"
Require (Join-Path $bin "TestApp\CyGameCaptureTestApp.exe") "l'application de test D3D11"
Require (Join-Path $bin "TestAppD3D12\CyGameCaptureTestAppD3D12.exe") "l'application de test D3D12"
Require (Join-Path $bin "CyGameCaptureSpoutReceiverTest.exe") "l'outil de reception Spout"

Write-Host ""
Write-Host "  Assemblage du dossier de test"
Write-Host "  vers $OutputDirectory"
Write-Host ""

if (Test-Path $OutputDirectory) {
    Remove-Item $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

function Section($name) { Write-Host ("  - " + $name) }

# ----------------------------------------------------------------- the add-on
Section "add-on ReShade"
$addon = Join-Path $OutputDirectory "Addon-ReShade"
New-Item -ItemType Directory -Force $addon | Out-Null
Copy-Item (Join-Path $bin "CyGameCaptureRS.addon64") $addon
# The AI assistant's helper; the add-on starts it from its own folder
Copy-Item (Join-Path $bin "CyGameCaptureAI.exe") $addon
if ($WithSymbols) { Copy-Item (Join-Path $bin "CyGameCaptureRS.pdb") $addon -ErrorAction SilentlyContinue }
Copy-Item (Join-Path $files "LISEZMOI-installer-dans-un-jeu.txt") $addon
Copy-Item (Join-Path $root "LICENSE") (Join-Path $addon "LICENSE.txt")

# ----------------------------------------------------------------- the OBS plugin
Section "plugin OBS"
$obs = Join-Path $OutputDirectory "Plugin-OBS"
New-Item -ItemType Directory -Force (Join-Path $obs "bin\64bit") | Out-Null
Copy-Item (Join-Path $bin "obs-plugins\64bit\CyGameCaptureOBS.dll") (Join-Path $obs "bin\64bit")
Copy-Item (Join-Path $bin "data\obs-plugins\CyGameCaptureOBS") (Join-Path $obs "data") -Recurse

# ----------------------------------------------------------------- the test applications
Section "applications de test"
$apps = Join-Path $OutputDirectory "Applications-de-test"
foreach ($pair in @(@("TestApp", "D3D11"), @("TestAppD3D12", "D3D12"))) {
    $source = Join-Path $bin $pair[0]
    $target = Join-Path $apps $pair[1]
    New-Item -ItemType Directory -Force $target | Out-Null
    # everything but the logs (ReShade keeps rotated ones: .log1, .log2...) and the ini, replaced by the demo one below
    Get-ChildItem $source -File | Where-Object { $_.Name -notmatch '\.(log\d*|ini)$' -and ($WithSymbols -or $_.Extension -ne '.pdb') } | ForEach-Object {
        Copy-Item $_.FullName $target
    }
    Copy-Item (Join-Path $files ("ini\ReShade-" + $pair[1] + ".ini")) (Join-Path $target "ReShade.ini")
}

# ----------------------------------------------------------------- the tools
Section "outils de verification"
$tools = Join-Path $OutputDirectory "Outils"
New-Item -ItemType Directory -Force $tools | Out-Null
Copy-Item (Join-Path $bin "CyGameCaptureSpoutReceiverTest.exe") $tools
if (Test-Path (Join-Path $bin "CyGameCaptureVideoProbe.exe")) {
    Copy-Item (Join-Path $bin "CyGameCaptureVideoProbe.exe") $tools
} else {
    Write-Host "      (la sonde d'enregistrement est absente : Tools\GenerateFFmpegImportLibs.cmd puis rebuild)"
}

# ----------------------------------------------------------------- the scene collections
Section "collections de scenes OBS"
$scenes = Join-Path $OutputDirectory "Scenes-OBS"
New-Item -ItemType Directory -Force $scenes | Out-Null
& (Join-Path $PSScriptRoot "MakeTestScenes.ps1") -OutputDirectory $scenes

# ----------------------------------------------------------------- the Unreal plugin
if (-not $NoUnreal) {
    $packages = Get-ChildItem (Join-Path $root "build") -Directory -Filter "Plugin_UE*" -ErrorAction SilentlyContinue
    if ($packages) {
        Section "plugin Unreal"
        $unreal = Join-Path $OutputDirectory "Plugin-Unreal"
        foreach ($package in $packages) {
            $version = $package.Name -replace '^Plugin_UE', ''
            $target = Join-Path $unreal ("CyGameCaptureUE-" + $version)
            New-Item -ItemType Directory -Force $target | Out-Null
            # Intermediate holds build leftovers only; it is big and of no use to anyone installing
            $exclude = if ($WithSymbols) { @() } else { @('/XF', '*.pdb') }
            robocopy $package.FullName $target /E /XD Intermediate @exclude /NFL /NDL /NJH /NJS /NP | Out-Null
            Write-Host ("      Unreal " + $version)
        }
    }
}

# ----------------------------------------------------------------- documentation
Section "documentation"
$docs = Join-Path $OutputDirectory "Documentation"
New-Item -ItemType Directory -Force $docs | Out-Null
Copy-Item (Join-Path $root "README.md") (Join-Path $docs "CyGameCapture - README.md")
Copy-Item (Join-Path $root "Docs\CyGameCapture_Architecture.md") $docs
Copy-Item (Join-Path $root "CyGameCaptureOBS\README.md") (Join-Path $docs "CyGameCaptureOBS - README.md")
Copy-Item (Join-Path $root "CyGameCaptureUE\README.md") (Join-Path $docs "CyGameCaptureUE - README.md") -ErrorAction SilentlyContinue
# The README shows the logo through a relative path, which has to resolve from here as well
$branding = Join-Path $root "Resources\Branding"
New-Item -ItemType Directory -Force (Join-Path $docs "Resources\Branding") | Out-Null
Copy-Item (Join-Path $branding "CyGameCapture_Logo_256.png") (Join-Path $docs "Resources\Branding")
# and the logo and icon themselves at the top of the package
Copy-Item (Join-Path $branding "CyGameCapture_Logo_512.png") (Join-Path $OutputDirectory "CyGameCapture.png")
Copy-Item (Join-Path $branding "CyGameCapture.ico") $OutputDirectory

$notices = Join-Path $docs "Licences-tierces"
New-Item -ItemType Directory -Force $notices | Out-Null
Copy-Item (Join-Path $root "CyGameCaptureRS\ThirdParty\THIRD_PARTY_NOTICES.md") (Join-Path $notices "CyGameCaptureRS.md")
Copy-Item (Join-Path $root "CyGameCaptureOBS\ThirdParty\THIRD_PARTY_NOTICES.md") (Join-Path $notices "CyGameCaptureOBS.md")
Copy-Item (Join-Path $root "Tests\CyGameCaptureVideoProbe\ThirdParty\THIRD_PARTY_NOTICES.md") (Join-Path $notices "CyGameCaptureVideoProbe.md") -ErrorAction SilentlyContinue
# ReShade itself is shipped next to the test applications, so its licence travels with it
Copy-Item (Join-Path $root "CyGameCaptureRS\ThirdParty\reshade\LICENSE.md") (Join-Path $notices "ReShade - LICENSE.md")

# ----------------------------------------------------------------- the scripts and the guide
Section "scripts et guide"
Get-ChildItem $files -Filter "*.cmd" | ForEach-Object { Copy-Item $_.FullName $OutputDirectory }
Copy-Item (Join-Path $files "LISEZMOI.txt") $OutputDirectory
Copy-Item (Join-Path $root "LICENSE") (Join-Path $OutputDirectory "LICENSE.txt")

# ----------------------------------------------------------------- a note on what this build is
$addonVersion = (Get-Item (Join-Path $bin "CyGameCaptureRS.addon64")).VersionInfo.FileVersion
$built = Get-Date -Format "yyyy-MM-dd HH:mm"
@(
    "CyGameCapture - dossier de test",
    "",
    "Assemble le $built",
    "Add-on ReShade      $addonVersion",
    "Plugin OBS          compile pour libobs 29.1.3",
    "",
    "Genere par Tools\MakeTestPackage.cmd depuis bin\Release.",
    "Relancez ce script apres chaque build pour rafraichir le dossier."
) | Set-Content (Join-Path $OutputDirectory "VERSION.txt") -Encoding utf8

$size = (Get-ChildItem $OutputDirectory -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ""
Write-Host ("  Termine : {0:N1} Mo" -f $size)
Write-Host "  $OutputDirectory"
Write-Host ""
