@echo off
setlocal enabledelayedexpansion
rem ---------------------------------------------------------------------------
rem  GenerateObsImportLib.cmd [path\to\obs.dll]
rem
rem  OBS Studio ships no import library for obs.dll, and building the whole
rem  obs-studio tree just to link a plugin is not worth it. This reads the
rem  exports of the installed obs.dll and produces
rem  CyGameCaptureOBS\ThirdParty\obs-studio\lib\obs.lib from them.
rem
rem  Run it once after installing OBS (or after upgrading it to a version with
rem  a different libobs API major number).
rem ---------------------------------------------------------------------------
set ROOT=%~dp0..
set OBS_DLL=%1
if "%OBS_DLL%"=="" set OBS_DLL=C:\Program Files\obs-studio\bin\64bit\obs.dll
if not exist "%OBS_DLL%" (
    echo obs.dll not found: "%OBS_DLL%"
    echo Pass its path as the first argument.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
    echo Visual Studio with C++ tools not found
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set OUT_DIR=%ROOT%\CyGameCaptureOBS\ThirdParty\obs-studio\lib
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
set DEF_FILE=%OUT_DIR%\obs.def

echo Reading exports of "%OBS_DLL%" ...
dumpbin /nologo /exports "%OBS_DLL%" > "%TEMP%\cygc_obs_exports.txt" || exit /b 1

> "%DEF_FILE%" echo LIBRARY obs
>> "%DEF_FILE%" echo EXPORTS
for /f "usebackq tokens=1,2,3,4" %%a in ("%TEMP%\cygc_obs_exports.txt") do (
    rem Export lines look like: <ordinal> <hint> <RVA> <name> = <name>
    echo %%a| findstr /r "^[0-9][0-9]*$" >nul && if not "%%d"=="" >> "%DEF_FILE%" echo     %%d
)

lib /nologo /def:"%DEF_FILE%" /machine:x64 /out:"%OUT_DIR%\obs.lib" || exit /b 1
del "%TEMP%\cygc_obs_exports.txt" >nul 2>&1
del "%DEF_FILE%" "%OUT_DIR%\obs.exp" >nul 2>&1
rem (only the .lib is kept: the .def is just the input to lib.exe and the .exp its by-product)
echo.
echo Generated "%OUT_DIR%\obs.lib"
endlocal
