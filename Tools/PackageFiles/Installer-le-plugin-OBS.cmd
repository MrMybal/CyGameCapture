@echo off
setlocal
rem ---------------------------------------------------------------------------
rem  Installe le plugin CyGameCaptureOBS pour l'utilisateur courant.
rem  Aucun droit administrateur necessaire : OBS cherche les plugins utilisateur
rem  dans %ProgramData%\obs-studio\plugins (et non dans %APPDATA%).
rem ---------------------------------------------------------------------------
set SRC=%~dp0Plugin-OBS
set DST=%ProgramData%\obs-studio\plugins\CyGameCaptureOBS

echo.
echo   Installation du plugin OBS
echo   vers %DST%
echo.

tasklist /fi "imagename eq obs64.exe" | find /i "obs64.exe" >nul
if not errorlevel 1 (
    echo   ATTENTION : OBS est ouvert. Fermez-le avant d'installer, sinon il ne
    echo   verra pas le plugin et pourrait ecraser la configuration en quittant.
    echo.
    pause
)

if not exist "%SRC%\bin\64bit\CyGameCaptureOBS.dll" (
    echo   ERREUR : %SRC%\bin\64bit\CyGameCaptureOBS.dll introuvable.
    echo   Le dossier de test est-il complet ?
    pause
    exit /b 1
)

mkdir "%DST%\bin\64bit" 2>nul
mkdir "%DST%\data" 2>nul
xcopy /y /q "%SRC%\bin\64bit\*" "%DST%\bin\64bit\" >nul || goto :failed
xcopy /y /q /e "%SRC%\data\*" "%DST%\data\" >nul || goto :failed

echo   Installe.
echo.
echo   Lancez OBS, puis Sources ^> + ^> CyGameCapture
echo.
pause
exit /b 0

:failed
echo.
echo   ERREUR : la copie a echoue.
pause
exit /b 1
