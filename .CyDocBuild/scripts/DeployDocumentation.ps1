param([Parameter(Mandatory=$true)][string] $ProjectRoot, [Parameter(Mandatory=$true)][string] $Destination)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'PackageDocumentation.ps1')
Copy-UserGuide -ProjectRoot $ProjectRoot -Destination $Destination
