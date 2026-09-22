@echo off
rem ---------------------------------------------------------------------------
rem  MakeRelease.cmd [output folder]
rem
rem  Builds the release archives in dist\CyGameCapture-<version>: ReShade add-on,
rem  OBS plugin, Unreal plugin per engine version, a complete archive, the
rem  SHA-256 sums and the release notes.
rem
rem  Needs build.cmd Release, and RunUAT BuildPlugin into build\Plugin_UE<version>
rem  for each engine version (see CyGameCaptureUE\README.md).
rem ---------------------------------------------------------------------------
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0MakeRelease.ps1" -OutputDirectory "%~1"
if errorlevel 1 (
    echo.
    echo   The release could not be built.
    exit /b 1
)
