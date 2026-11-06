param([Parameter(Mandatory=$true)][string] $ReleaseDirectory)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $root 'Tools/PackageLicences.ps1')
$source=Join-Path $root 'Documentation'
$base=(Resolve-Path -LiteralPath $source).Path.TrimEnd('\')+'\'
$assets=@(Get-ChildItem -LiteralPath $source -Recurse -File)
$archives=@(Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*.zip')
if(-not $archives.Count){throw 'No release archives'}
foreach($archive in $archives){
    $zip=[IO.Compression.ZipFile]::OpenRead($archive.FullName)
    try{
        if($zip.Entries | Where-Object { $_.FullName -match '/\.CyDocBuild/|/Documentation/UserGuide/|/Docs/UserGuide/' }){throw 'Working or retired reader files were distributed'}
        $guides=@($zip.Entries | Where-Object { $_.FullName.EndsWith('/Documentation/Documentation.html') })
        if(-not $guides.Count){throw 'Missing Documentation.html'}
        foreach($guide in $guides){
            $prefix=$guide.FullName.Substring(0,$guide.FullName.Length-'Documentation.html'.Length)
            $actual=@($zip.Entries | Where-Object {$_.FullName.StartsWith($prefix) -and -not $_.FullName.EndsWith('/')})
            if($actual.Count -ne $assets.Count){throw "Unexpected files in $prefix"}
            foreach($asset in $assets){
                $name=$asset.FullName.Substring($base.Length).Replace('\','/');$entry=$zip.GetEntry($prefix+$name)
                if(-not $entry){throw "Missing $prefix$name"}
                $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
                try{$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$sha.Dispose();$stream.Dispose()}
                if($hash -ne (Get-ProductFileHash -Path $asset.FullName)){throw "Wrong $prefix$name"}
            }
        }
        if($archive.Name -match 'Complete' -and $guides.Count -ne 10){throw 'Missing component documentation'}
        Write-Host "PASS $($archive.Name): $($guides.Count) exact complete Documentation trees; no private workspace or retired reader."
    }finally{$zip.Dispose()}
}
