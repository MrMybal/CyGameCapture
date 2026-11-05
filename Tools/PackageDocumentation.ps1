# Deploy the complete CyDoc export, including providers, image module and notices.
function Copy-UserGuide {
    param([string] $ProjectRoot, [string] $Destination)
    $source = Join-Path $ProjectRoot 'Docs/UserGuide'
    if (-not (Test-Path -LiteralPath (Join-Path $source 'manifest.json'))) { throw 'Build the CyDoc manual first: npm run docs:build' }
    $target = [IO.Path]::GetFullPath($Destination).TrimEnd('\','/')
    if ([IO.Path]::GetFileName($target) -ne 'UserGuide' -or $target -eq [IO.Path]::GetFullPath($source)) { throw 'Unexpected manual deployment target' }
    if (Test-Path -LiteralPath $target) {
        $resolved = (Resolve-Path -LiteralPath $target).Path
        if ($resolved -ne $target -or ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing a linked deployment target' }
        if (Get-ChildItem -LiteralPath $target -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { throw 'Refusing linked manual assets' }
        if (-not (Test-Path -LiteralPath (Join-Path $target 'build-info.json')) -and -not (Test-Path -LiteralPath (Join-Path $target 'manual.js'))) { throw 'Refusing to replace an unmanaged directory' }
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Get-ChildItem -LiteralPath $source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $target -Recurse -Force
    }
}

function Copy-ReadmeImages {
    param([string] $ProjectRoot, [string] $DocumentationRoot)
    $target = Join-Path $DocumentationRoot 'Docs/Images'
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Docs/Images') -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $target -Force
    }
}

function Copy-DocumentationReadme {
    param([string] $ProjectRoot, [string] $DocumentationRoot)
    $text = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'README.md'))
    # The packaged README lives inside Documentation rather than the repository root.
    $text = $text.Replace('](Docs/UserGuide/', '](UserGuide/')
    [IO.File]::WriteAllText((Join-Path $DocumentationRoot 'CyGameCapture - README.md'), $text, [Text.UTF8Encoding]::new($false))
}
