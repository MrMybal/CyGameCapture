@echo off
setlocal
rem ---------------------------------------------------------------------------
rem  Ajoute deux collections de scenes pretes a l'emploi, une par application de
rem  test. Purement additif : vos collections existantes ne sont pas modifiees,
rem  et la collection active ne change pas.
rem ---------------------------------------------------------------------------
set SRC=%~dp0Scenes-OBS
set DST=%APPDATA%\obs-studio\basic\scenes

echo.
echo   Installation des collections de scenes de test
echo   vers %DST%
echo.

tasklist /fi "imagename eq obs64.exe" | find /i "obs64.exe" >nul
if not errorlevel 1 (
    echo   ATTENTION : OBS est ouvert. Fermez-le avant, sinon il reecrira ses
    echo   fichiers de configuration en quittant et effacera ces collections.
    echo.
    pause
)

if not exist "%DST%" mkdir "%DST%"
xcopy /y /q "%SRC%\*.json" "%DST%\" >nul || goto :failed

echo   Installe.
echo.
echo   Dans OBS : menu  Collection de scenes  ^>  CyGameCapture Test D3D11
echo                                          ou  CyGameCapture Test D3D12
echo.
pause
exit /b 0

:failed
echo.
echo   ERREUR : la copie a echoue.
pause
exit /b 1
