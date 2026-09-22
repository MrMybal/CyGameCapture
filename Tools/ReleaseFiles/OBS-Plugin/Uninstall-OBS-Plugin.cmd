@echo off
setlocal
rem ---------------------------------------------------------------------------
rem  Removes the CyGameCaptureOBS plugin installed by Install-OBS-Plugin.cmd.
rem  Touches nothing else: scene collections and OBS settings stay as they are.
rem ---------------------------------------------------------------------------
set DST=%ProgramData%\obs-studio\plugins\CyGameCaptureOBS

echo.
echo   Removing the CyGameCapture OBS plugin / Desinstallation du plugin OBS

tasklist /fi "imagename eq obs64.exe" | find /i "obs64.exe" >nul
if not errorlevel 1 (
    echo.
    echo   OBS is running: close it first, then press a key.
    echo   OBS est ouvert : fermez-le d'abord, puis appuyez sur une touche.
    echo.
    pause
)

if exist "%DST%" (
    rmdir /s /q "%DST%"
    echo   Removed / Retire.
) else (
    echo   Not installed / Deja absent.
)
echo.
pause
