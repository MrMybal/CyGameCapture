# Copies and verifies complete notices for the components actually shipped.
# Dot-source from packaging scripts. Does not change user settings or runtimes.
function Get-ProductFileHash {
    param([Parameter(Mandatory = $true)][string] $Path)
    # Avoid depending on module discovery when Windows PowerShell is launched from PowerShell 7.
    $stream = [System.IO.File]::OpenRead($Path)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return [System.BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Copy-ProductLicences {
    param(
        [Parameter(Mandatory = $true)][string] $ProjectRoot,
        [Parameter(Mandatory = $true)][string] $Destination,
        [Parameter(Mandatory = $true)]
        [ValidateSet('ReShade', 'OBS', 'Unreal', 'Complete', 'Test')][string] $Component,
        [switch] $IncludeVideoProbe
    )
    $source = Join-Path $ProjectRoot 'LICENCES'
    $names = @('SOURCES.txt', 'Spout2-LICENSE.txt', 'Spout2-licence.txt')
    switch ($Component) {
        'ReShade' { $names += @('CyGameCapture-AGPL-3.0.txt', 'CyGameCaptureRS-Notices.md', 'ReShade-LICENSE.md', 'DearImGui-MIT.txt') }
        'OBS' { $names += @('CyGameCapture-AGPL-3.0.txt', 'CyGameCaptureOBS-Notices.md', 'OBS-Studio-COPYING.txt', 'OBS-Studio-COMMITMENT.txt') }
        'Unreal' {
            $source = Join-Path $ProjectRoot 'CyGameCaptureUE\LICENCES'
            $names += @('CyGameCaptureUE-MIT.txt', 'CyGameCaptureUE-Notices.md')
        }
        default {
            $names += @('CyGameCapture-AGPL-3.0.txt', 'CyGameCaptureUE-MIT.txt',
                'CyGameCaptureRS-Notices.md', 'CyGameCaptureOBS-Notices.md', 'CyGameCaptureUE-Notices.md',
                'ReShade-LICENSE.md', 'DearImGui-MIT.txt', 'OBS-Studio-COPYING.txt', 'OBS-Studio-COMMITMENT.txt')
        }
    }
    if ($IncludeVideoProbe) {
        $names += @('FFmpeg-LGPL-2.1.txt', 'FFmpeg-LICENSE.md', 'CyGameCaptureVideoProbe-Notices.md')
    }
    $target = Join-Path $Destination 'LICENCES'
    # Normalize the spelling of older directories on case-insensitive Windows.
    $existing = Get-ChildItem -LiteralPath $Destination -Directory | Where-Object { $_.Name -ieq 'LICENCES' }
    if ($existing -and $existing.Name -cne 'LICENCES') {
        $temporaryName = 'licences-case-' + [guid]::NewGuid().ToString('N')
        Rename-Item -LiteralPath $existing.FullName -NewName $temporaryName
        Rename-Item -LiteralPath (Join-Path $Destination $temporaryName) -NewName 'LICENCES'
    }
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    foreach ($name in $names) {
        $original = Join-Path $source $name
        $copy = Join-Path $target $name
        Copy-Item -LiteralPath $original -Destination $copy -Force
        if ((Get-ProductFileHash -Path $original) -ne (Get-ProductFileHash -Path $copy)) {
            throw "Packaged licence differs from source: $copy"
        }
    }
}
