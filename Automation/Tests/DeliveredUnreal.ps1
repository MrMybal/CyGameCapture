# Local runtime check of the actual exported ZIP, without opening a user's project.
param([string[]]$Versions=@('5.3','5.8'),[string]$ReleaseDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(-not $ReleaseDirectory){$ReleaseDirectory=Join-Path $root 'dist/CyGameCapture-CyMCPSDK-20261119-local'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach($version in $Versions) {
    if($version -notmatch '^5\.[3-8]$'){throw 'Expected a supported Unreal version'}
    $project=Join-Path $root ('build/cymcp-delivered-UE'+$version+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
    $plugins=Join-Path $project 'Plugins'
    New-Item -ItemType Directory -Force $plugins | Out-Null
    $archive=Join-Path $ReleaseDirectory "CyGameCapture-0.1.0-UnrealPlugin-UE$version-Win64.zip"
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    try {
        foreach($entry in $zip.Entries){$target=[IO.Path]::GetFullPath((Join-Path $plugins $entry.FullName));if(-not $target.StartsWith($plugins+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Archive traversal refused'}}
    }finally{$zip.Dispose()}
    [IO.Compression.ZipFile]::ExtractToDirectory($archive,$plugins)
    $uproject=Join-Path $project 'SDKDelivery.uproject'
    [IO.File]::WriteAllText($uproject,'{"FileVersion":3,"Plugins":[{"Name":"CyGameCaptureUE","Enabled":true},{"Name":"PythonScriptPlugin","Enabled":true},{"Name":"EditorScriptingUtilities","Enabled":true}]}')
    $output=Join-Path $project 'Verification';New-Item -ItemType Directory $output | Out-Null
    $env:CYGC_TEST_ROOT=$root;$env:CYGC_TEST_OUTPUT=$output
    $env:LOCALAPPDATA=Join-Path $project 'UserData';$env:APPDATA=Join-Path $project 'Roaming'
    $env:CYGC_TEST_CLI=Join-Path $plugins 'CyGameCaptureUE/Binaries/ThirdParty/CyMCPSDK/Win64/CyGameCaptureCLI.exe'
    $engine=(Get-ItemProperty -LiteralPath "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$version").InstalledDirectory
    $arguments=@(('"'+$uproject+'"'),'-nullrhi','-unattended','-nosplash','-NoSound','-NoP4',('-UserDir="'+(Join-Path $project 'EditorUser')+'"'),('-ExecCmds="py '+(Join-Path $root 'Automation/Tests/Unreal.py').Replace('\','/')+'"'),('-Log="'+(Join-Path $output 'Unreal.log')+'"'))
    $child=Start-Process -FilePath (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput (Join-Path $output 'stdout.txt') -RedirectStandardError (Join-Path $output 'stderr.txt') -PassThru
    try {
        $deadline=[DateTime]::UtcNow.AddSeconds(120)
        while(-not $child.WaitForExit(500)){if([DateTime]::UtcNow -gt $deadline){throw "Unreal $version test timed out; inspect $output"}}
        $result=Get-Content -LiteralPath (Join-Path $output 'result.txt') -Raw
        if($result -notmatch '^PASS '){throw $result}
        Write-Host "PASS extracted UE${version} package and shipped CLI: $($result.Trim())"
    } finally {
        if(-not $child.HasExited){Stop-Process -Id $child.Id -Force}
    }
}
