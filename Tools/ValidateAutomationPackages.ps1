# Verify the SDK runtime actually delivered, without exposing local connection credentials.
param([Parameter(Mandatory=$true)][string]$PackageDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'PackageLicences.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$expected=@{}
foreach($name in 'CyGameCaptureMCP.dll','CyGameCaptureCLI.exe') {$expected[$name]=Get-ProductFileHash (Join-Path $root "bin/Release/$name")}
function Assert-Delivered($names,$getHash) {
    $checked=0
    foreach($name in $names) {
        if($name -match '(?i)(^|/)(UserData|node_modules|AssistantRuntime)/|(^|/)(request|current|profile|ui-history)\.json$|(^|/)(reshade|unreal|obs-source)-\d+-[a-f0-9]+\.json$|performance-.*\.jsonl$|\.pyc$') {throw "Private or development data in package: $name"}
        $leaf=$name.Split('/')[-1]
        if($expected.ContainsKey($leaf)) {
            if((& $getHash $name) -ne $expected[$leaf]) {throw "Stale SDK binary in package: $name"}
            $checked++
        }
        if($leaf -eq 'CyGameCaptureRS.addon64' -or $leaf -eq 'CyGameCaptureOBS.dll') {
            $prefix=$name.Substring(0,$name.LastIndexOf('/')+1)
            foreach($dependency in 'CyGameCaptureMCP.dll','CyGameCaptureCLI.exe') {if($names -notcontains ($prefix+$dependency)){throw "Missing native dependency: $prefix$dependency"}}
        }
        if($leaf -eq 'CyGameCaptureUE.uplugin') {
            $prefix=$name.Substring(0,$name.LastIndexOf('/')+1)+'Binaries/ThirdParty/CyMCPSDK/Win64/'
            foreach($dependency in 'CyGameCaptureMCP.dll','CyGameCaptureCLI.exe') {if($names -notcontains ($prefix+$dependency)){throw "Missing Unreal runtime: $prefix$dependency"}}
        }
    }
    if($checked -eq 0){throw 'No SDK runtime was delivered'}
    return $checked
}
$archives=@(Get-ChildItem -LiteralPath $PackageDirectory -Filter '*.zip')
if($archives.Count) {
    foreach($archive in $archives) {
        $zip=[IO.Compression.ZipFile]::OpenRead($archive.FullName)
        try {
            $names=@($zip.Entries | ForEach-Object {$_.FullName})
            $getHash={param($name) $entry=$zip.GetEntry($name);$stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create();try{[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$sha.Dispose();$stream.Dispose()}}
            $count=Assert-Delivered $names $getHash
            Write-Host "PASS $($archive.Name): $count exact SDK binaries; no private application data"
        } finally {$zip.Dispose()}
    }
} else {
    $base=(Resolve-Path -LiteralPath $PackageDirectory).Path.TrimEnd('\')+'\'
    $names=@(Get-ChildItem -LiteralPath $base -Recurse -File | ForEach-Object {$_.FullName.Substring($base.Length).Replace('\','/')})
    $count=Assert-Delivered $names {param($name) Get-ProductFileHash (Join-Path $base $name)}
    Write-Host "PASS local test package: $count exact SDK binaries; no private application data"
}
