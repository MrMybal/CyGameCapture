@echo off
setlocal
rem ---------------------------------------------------------------------------
rem  Retire le plugin CyGameCaptureOBS et les collections de scenes de test.
rem  Ne touche a aucune autre collection ni a la configuration d'OBS.
rem ---------------------------------------------------------------------------
set DST=%ProgramData%\obs-studio\plugins\CyGameCaptureOBS
set SCENES=%APPDATA%\obs-studio\basic\scenes

echo.
echo   Desinstallation de CyGameCapture

tasklist /fi "imagename eq obs64.exe" | find /i "obs64.exe" >nul
if not errorlevel 1 (
    echo.
    echo   ATTENTION : OBS est ouvert. Fermez-le d'abord.
    echo.
    pause
)

if exist "%DST%" (
    rmdir /s /q "%DST%"
    echo   - plugin retire
) else (
    echo   - plugin deja absent
)

for %%f in ("CyGameCapture Test D3D11" "CyGameCapture Test D3D12") do (
    if exist "%SCENES%\%%~f.json" (
        del "%SCENES%\%%~f.json"
        echo   - collection de scenes "%%~f" retiree
    )
    if exist "%SCENES%\%%~f.json.bak" del "%SCENES%\%%~f.json.bak"
)

echo.
echo   Si OBS etait sur une de ces collections, il reviendra sur une autre au
echo   prochain demarrage. Vos propres collections n'ont pas ete touchees.
echo.
pause
