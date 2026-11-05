# Shared deployment for the offline user guide. No network or generator is needed.
function Copy-UserGuide {
    param([string] $ProjectRoot, [string] $Destination)
    $source = Join-Path $ProjectRoot 'Docs/UserGuide'
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    foreach ($name in 'index.html', 'en.html', 'fr.html', 'style.css', 'manual.js') {
        Copy-Item -LiteralPath (Join-Path $source $name) -Destination $Destination -Force
    }
    $images = Join-Path $Destination 'images'
    New-Item -ItemType Directory -Force -Path $images | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $source 'images') -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $images -Force
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
