@echo off
setlocal
rem ---------------------------------------------------------------------------
rem  Installs the CyGameCaptureOBS plugin for the current user.
rem  No administrator rights needed: on Windows OBS looks for user plugins in
rem  %ProgramData%\obs-studio\plugins (not in %APPDATA%).
rem ---------------------------------------------------------------------------
set SRC=%~dp0CyGameCaptureOBS
set DST=%ProgramData%\obs-studio\plugins\CyGameCaptureOBS

echo.
echo   Installing the CyGameCapture OBS plugin / Installation du plugin OBS
echo   to / vers %DST%
echo.

tasklist /fi "imagename eq obs64.exe" | find /i "obs64.exe" >nul
if not errorlevel 1 (
    echo   OBS is running: close it first, then press a key.
    echo   OBS est ouvert : fermez-le d'abord, puis appuyez sur une touche.
    echo.
    pause
)

if not exist "%SRC%\bin\64bit\CyGameCaptureOBS.dll" (
    echo   ERROR / ERREUR: %SRC%\bin\64bit\CyGameCaptureOBS.dll not found / introuvable.
    echo   Extract the whole archive first / Extrayez d'abord toute l'archive.
    pause
    exit /b 1
)

mkdir "%DST%\bin\64bit" 2>nul
mkdir "%DST%\data" 2>nul
xcopy /y /q "%SRC%\bin\64bit\*" "%DST%\bin\64bit\" >nul || goto :failed
xcopy /y /q /e "%SRC%\data\*" "%DST%\data\" >nul || goto :failed

echo   Installed / Installe.
echo.
echo   Start OBS, then Sources ^> + ^> CyGameCapture
echo   Lancez OBS, puis Sources ^> + ^> CyGameCapture
echo.
pause
exit /b 0

:failed
echo.
echo   ERROR / ERREUR: the copy failed / la copie a echoue.
pause
exit /b 1
