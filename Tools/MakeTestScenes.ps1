# Writes the two ready-to-import OBS scene collections of the test package.
#
# Each one holds three CyGameCapture sources bound to the three senders the
# matching test application publishes, already set to the encoding that suits
# the buffer: lossless RGB for the colour and the HUD (the only mode with an
# alpha channel, which is where the HUD mask lives), lossless 16-bit for depth.
#
# No recording folder is set on purpose: left empty, the plugin uses its own
# default, %USERPROFILE%\Videos\CyGameCapture, which keeps the file portable.
param([string] $OutputDirectory)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

# 0 = hardware encoder, 1 = lossless 8-bit RGB (keeps alpha), 2 = lossless 16-bit
function New-Source($name, $sender, $mode) {
    @{
        prev_ver = 486539267
        name = $name
        id = "cygamecapture_source"
        versioned_id = "cygamecapture_source"
        settings = @{
            auto_connect = $false
            sender_name = $sender
            show_all_senders = $false
            hold_last_frame = $true
            record_buffer = $false      # off by default: a take is a deliberate act
            record_container = "mkv"
            record_audio = 1
            record_autostart = $false
            record_mode = $mode
        }
        mixers = 0; sync = 0; flags = 0; volume = 1.0; balance = 0.5
        enabled = $true; muted = $false
        "push-to-mute" = $false; "push-to-mute-delay" = 0
        "push-to-talk" = $false; "push-to-talk-delay" = 0
        hotkeys = @{}; deinterlace_mode = 0; deinterlace_field_order = 0
        monitoring_type = 0; private_settings = @{}
    }
}

function New-Item2($name, $index) {
    @{
        name = $name; source_cx = 1280; source_cy = 720
        # A 2x2 grid of 960x540 tiles: exactly a 1920x1080 canvas, the common case.
        # On another canvas size the tiles are simply dragged into place.
        pos = @{ x = [double](($index % 2) * 960); y = [double]([math]::Floor($index / 2) * 540) }
        scale = @{ x = 0.75; y = 0.75 }
        align = 5; bounds_type = 0; bounds_align = 0; bounds = @{ x = 0.0; y = 0.0 }
        id = $index + 1; group_item_backup = $false
        scale_filter = "disable"; blend_method = "default"; blend_type = "normal"
        visible = $true; locked = $false; rot = 0.0
        crop_left = 0; crop_top = 0; crop_right = 0; crop_bottom = 0
        hide_transition = @{ duration = 0 }; show_transition = @{ duration = 0 }
        private_settings = @{}
    }
}

function Write-Collection($collectionName, $baseSender, $path) {
    $streams = @(
        @("1 - Scene sans ATH", $baseSender, 1),
        @("2 - Depth", ($baseSender + "::2"), 2),
        @("3 - ATH seul", ($baseSender + "::3"), 1)
    )

    $sources = @()
    $items = @()
    for ($i = 0; $i -lt $streams.Count; $i++) {
        $sources += New-Source $streams[$i][0] $streams[$i][1] $streams[$i][2]
        $items += New-Item2 $streams[$i][0] $i
    }

    $scene = @{
        prev_ver = 486539267; name = "Scene"; id = "scene"; versioned_id = "scene"
        settings = @{ id_counter = $streams.Count; custom_size = $false; items = $items }
        mixers = 0; sync = 0; flags = 0; volume = 1.0; balance = 0.5
        enabled = $true; muted = $false
        "push-to-mute" = $false; "push-to-mute-delay" = 0
        "push-to-talk" = $false; "push-to-talk-delay" = 0
        hotkeys = @{}; deinterlace_mode = 0; deinterlace_field_order = 0
        monitoring_type = 0; private_settings = @{}
    }

    $collection = @{
        current_scene = "Scene"; current_program_scene = "Scene"
        scene_order = @(@{ name = "Scene" })
        name = $collectionName
        groups = @(); quick_transitions = @(); transitions = @()
        current_transition = "Fondu"; transition_duration = 300
        preview_locked = $false; scaling_enabled = $false
        sources = $sources + @($scene)
    }

    # Without a BOM on purpose: Set-Content -Encoding utf8 adds one on Windows
    # PowerShell, and a leading BOM is not valid JSON for every parser.
    $json = $collection | ConvertTo-Json -Depth 12
    [System.IO.File]::WriteAllText($path, $json, (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("      " + (Split-Path -Leaf $path))
}

Write-Collection "CyGameCapture Test D3D11" "CyGameCaptureRS::CyGameCaptureTestApp" `
    (Join-Path $OutputDirectory "CyGameCapture Test D3D11.json")
Write-Collection "CyGameCapture Test D3D12" "CyGameCaptureRS::CyGameCaptureTestAppD3D12" `
    (Join-Path $OutputDirectory "CyGameCapture Test D3D12.json")
