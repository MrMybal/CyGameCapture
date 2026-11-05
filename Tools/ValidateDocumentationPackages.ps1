param([Parameter(Mandatory=$true)][string] $ReleaseDirectory)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
. (Join-Path $PSScriptRoot 'PackageLicences.ps1')
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'Docs/UserGuide'
$sourceBase = (Resolve-Path -LiteralPath $source).Path.TrimEnd('\') + '\'
$assets = @(Get-ChildItem -LiteralPath $source -Recurse -File)
foreach ($archive in Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*.zip') {
    $zip = [IO.Compression.ZipFile]::OpenRead($archive.FullName)
    try {
        $guides = @($zip.Entries | Where-Object { $_.FullName.EndsWith('/UserGuide/index.html') })
        if ($guides.Count -eq 0) { throw "Missing CyDoc reader: $($archive.Name)" }
        foreach ($index in $guides) {
            $prefix = $index.FullName.Substring(0, $index.FullName.Length - 'index.html'.Length)
            $actual = @($zip.Entries | Where-Object { $_.FullName.StartsWith($prefix) -and -not $_.FullName.EndsWith('/') })
            if ($actual.Count -ne $assets.Count) { throw "Incomplete or stale reader: $prefix" }
            foreach ($asset in $assets) {
                $name = $asset.FullName.Substring($sourceBase.Length).Replace('\','/')
                $entry = $zip.GetEntry($prefix + $name)
                if (-not $entry) { throw "Missing exported resource: $prefix$name" }
                $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
                try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
                finally { $sha.Dispose(); $stream.Dispose() }
                if ($hash -ne (Get-ProductFileHash -Path $asset.FullName)) { throw "Wrong exported resource: $prefix$name" }
            }
        }
        if ($archive.Name -match 'Complete' -and $guides.Count -ne 10) { throw 'Complete archive is missing component readers' }
        if ($archive.Name -match 'OBS-Plugin' -and -not ($guides | Where-Object { $_.FullName -like '*/CyGameCaptureOBS/data/Documentation/UserGuide/index.html' })) { throw 'OBS installed data has no reader' }
        if ($archive.Name -match 'UnrealPlugin' -and -not ($guides | Where-Object { $_.FullName -like '*/Docs/UserGuide/index.html' })) { throw 'Unreal reader missing' }
        Write-Host "PASS $($archive.Name): $($guides.Count) complete, exact CyDoc reader(s)"
    } finally { $zip.Dispose() }
}
