# Deploy the complete CyDoc export, including providers, image module and notices.
function Copy-UserGuide {
    param([string] $ProjectRoot, [string] $Destination)
    $source = Join-Path $ProjectRoot 'Documentation'
    if (-not (Test-Path -LiteralPath (Join-Path $source 'DocData/manifest.json'))) { throw 'Build the manual first: npm --prefix .CyDocBuild run docs:build' }
    $target = [IO.Path]::GetFullPath($Destination).TrimEnd('\','/')
    $projectBase = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\') + '\'
    if ([IO.Path]::GetFileName($target) -ne 'Documentation' -or $target -eq [IO.Path]::GetFullPath($source) -or -not $target.StartsWith($projectBase,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected manual deployment target' }
    if (Test-Path -LiteralPath $target) {
        $resolved = (Resolve-Path -LiteralPath $target).Path
        if ($resolved -ne $target -or ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing a linked deployment target' }
        if (Get-ChildItem -LiteralPath $target -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { throw 'Refusing linked manual assets' }
        foreach ($childName in @('DocData','UserGuide')) {
            $child = Join-Path $target $childName
            if (Test-Path -LiteralPath $child) {
                $marker = if($childName -eq 'DocData') { Join-Path $child 'manifest.json' } else { Join-Path $child 'build-info.json' }
                if(-not (Test-Path -LiteralPath $marker)) { throw 'Refusing to replace unmanaged documentation data' }
                $resolvedChild = (Resolve-Path -LiteralPath $child).Path
                if(-not $resolvedChild.StartsWith($target + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected generated child' }
                Remove-Item -LiteralPath $resolvedChild -Recurse -Force
            }
        }
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
    # The packaged README lives inside ProductInformation rather than the repository root.
    $text = $text.Replace('](Documentation/', '](../Documentation/')
    [IO.File]::WriteAllText((Join-Path $DocumentationRoot 'CyGameCapture - README.md'), $text, [Text.UTF8Encoding]::new($false))
}
