@echo off
setlocal enabledelayedexpansion
rem ---------------------------------------------------------------------------
rem  GenerateFFmpegImportLibs.cmd [path\to\obs-studio\bin\64bit]
rem
rem  CyGameCaptureVideoProbe decodes recordings with the FFmpeg libraries OBS
rem  Studio already ships, so nothing extra has to be installed. OBS ships the
rem  DLLs but no import libraries, so this reads their exports and produces
rem  Tests\CyGameCaptureVideoProbe\ThirdParty\ffmpeg\lib\*.lib from them.
rem
rem  Run it once after installing OBS. Without it, CMake simply skips the probe.
rem ---------------------------------------------------------------------------
set ROOT=%~dp0..
set OBS_BIN=%1
if "%OBS_BIN%"=="" set OBS_BIN=C:\Program Files\obs-studio\bin\64bit
if not exist "%OBS_BIN%\avcodec-60.dll" (
    echo FFmpeg libraries not found in "%OBS_BIN%"
    echo Pass the path of the obs-studio\bin\64bit folder as the first argument.
    echo.
    echo Note the version: this expects the FFmpeg 6.0 libraries of OBS 29.x
    echo ^(avcodec-60, avformat-60, avutil-58^). A different OBS may ship other
    echo ones, in which case the vendored headers have to match them too.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
    echo Visual Studio with C++ tools not found
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set OUT_DIR=%ROOT%\Tests\CyGameCaptureVideoProbe\ThirdParty\ffmpeg\lib
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

call :make avcodec-60  avcodec  || exit /b 1
call :make avformat-60 avformat || exit /b 1
call :make avutil-58   avutil   || exit /b 1
echo.
echo Generated the import libraries in "%OUT_DIR%"
exit /b 0

:make
set DLL=%1
set NAME=%2
echo Reading exports of %DLL%.dll ...
dumpbin /nologo /exports "%OBS_BIN%\%DLL%.dll" > "%TEMP%\cygc_%NAME%.txt" || exit /b 1
> "%OUT_DIR%\%NAME%.def" echo LIBRARY %DLL%
>> "%OUT_DIR%\%NAME%.def" echo EXPORTS
for /f "usebackq tokens=1,2,3,4" %%a in ("%TEMP%\cygc_%NAME%.txt") do (
    rem Export lines look like: <ordinal> <hint> <RVA> <name> = <name>
    echo %%a| findstr /r "^[0-9][0-9]*$" >nul && if not "%%d"=="" >> "%OUT_DIR%\%NAME%.def" echo     %%d
)
lib /nologo /def:"%OUT_DIR%\%NAME%.def" /machine:x64 /out:"%OUT_DIR%\%NAME%.lib" || exit /b 1
del "%TEMP%\cygc_%NAME%.txt" >nul 2>&1
del "%OUT_DIR%\%NAME%.def" "%OUT_DIR%\%NAME%.exp" >nul 2>&1
rem (only the .lib is kept: the .def is just the input to lib.exe and the .exp its by-product)
exit /b 0
