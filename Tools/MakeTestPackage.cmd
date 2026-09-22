@echo off
rem ---------------------------------------------------------------------------
rem  MakeTestPackage.cmd [dossier de sortie] [noue]
rem
rem  Assemble le dossier de test pret a l'emploi depuis bin\Release :
rem  add-on ReShade, plugin OBS, applications de test, outils, scenes OBS,
rem  plugin Unreal, documentation et scripts d'installation.
rem
rem  Sans argument, il ecrit dans dist\CyGameCapture_Test.
rem  Passer "noue" en deuxieme argument pour laisser de cote le plugin Unreal,
rem  qui pese a lui seul plus de 200 Mo.
rem
rem  Lancer build.cmd Release avant.
rem ---------------------------------------------------------------------------
setlocal
set EXTRA=
if /i "%~2"=="noue" set EXTRA=-NoUnreal
if /i "%~1"=="noue" set EXTRA=-NoUnreal
if /i "%~1"=="noue" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0MakeTestPackage.ps1" %EXTRA%
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0MakeTestPackage.ps1" -OutputDirectory "%~1" %EXTRA%
)
if errorlevel 1 (
    echo.
    echo   L'assemblage a echoue.
    exit /b 1
)
