@echo off
rem Builds the Pixel LABS VST3 plugin, the test host and the release packages.
rem
rem   build.bat          build plugin + installer + portable zip into dist\
rem   build.bat test     same, then run the automated host test (renders demos + screenshots)
rem
rem Needs Visual Studio 2022 or newer with the "Desktop development with C++" workload.
setlocal EnableExtensions

set "VERSION=1.1.0"
rem ^ keep in sync with src\plugin\Version.h

set "ROOT=%~dp0"
set "BUILD=%ROOT%build"
set "DIST=%ROOT%dist"
set "BUNDLE=%DIST%\PixelLABS.vst3"
set "PLUGIN=%BUNDLE%\Contents\x86_64-win\PixelLABS.vst3"

where cl.exe >nul 2>nul
if not errorlevel 1 goto have_msvc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_msvc
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto no_msvc
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
where cl.exe >nul 2>nul
if errorlevel 1 goto no_msvc

:have_msvc
if not exist "%BUILD%\obj" mkdir "%BUILD%\obj"
if not exist "%BUILD%\installer" mkdir "%BUILD%\installer"
if not exist "%BUNDLE%\Contents\x86_64-win" mkdir "%BUNDLE%\Contents\x86_64-win"

set "CXXFLAGS=/nologo /std:c++17 /O2 /EHsc /MD /W3 /DNDEBUG /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%include""

echo [1/4] Plugin
cl.exe %CXXFLAGS% /Fo"%BUILD%\obj\\" ^
    "%ROOT%src\dsp\RetroSynthEngine.cpp" ^
    "%ROOT%src\gui\PixelLabsView.cpp" ^
    "%ROOT%src\library\SoundLibrary.cpp" ^
    "%ROOT%src\plugin\PixelLabsPlugin.cpp" ^
    "%ROOT%src\plugin\dllmain.cpp" ^
    /LD /Fe"%PLUGIN%" ^
    /link /DEF:"%ROOT%src\plugin\vst3.def" /IMPLIB:"%BUILD%\obj\PixelLABS.lib" user32.lib gdi32.lib
if errorlevel 1 goto failed

echo [2/4] Test host
cl.exe %CXXFLAGS% /Fo"%BUILD%\obj\\" "%ROOT%tests\test_vst3_host.cpp" /Fe"%BUILD%\test_vst3_host.exe" /link user32.lib gdi32.lib
if errorlevel 1 goto failed

echo [3/4] Installer
copy /Y "%PLUGIN%" "%BUILD%\installer\PixelLABS.vst3.bin" >nul
pushd "%BUILD%\installer"
rc.exe /nologo /i "%ROOT%installer" /i "%ROOT%src\plugin" /fo setup.res "%ROOT%installer\setup.rc"
set "RCERR=%ERRORLEVEL%"
popd
if not "%RCERR%"=="0" goto failed
cl.exe /nologo /std:c++17 /O2 /EHsc /MT /W3 /DNDEBUG /DUNICODE /D_UNICODE /Fo"%BUILD%\obj\\" ^
    "%ROOT%installer\PixelLabsSetup.cpp" "%BUILD%\installer\setup.res" ^
    /Fe"%DIST%\PixelLABS-Setup-%VERSION%.exe" ^
    /link /SUBSYSTEM:WINDOWS comctl32.lib shell32.lib ole32.lib advapi32.lib user32.lib
if errorlevel 1 goto failed

echo [4/4] Portable zip
set "STAGE=%BUILD%\zip\PixelLABS-%VERSION%"
if exist "%BUILD%\zip" rmdir /s /q "%BUILD%\zip"
mkdir "%STAGE%"
xcopy /E /I /Q /Y "%BUNDLE%" "%STAGE%\PixelLABS.vst3" >nul
copy /Y "%ROOT%LICENSE" "%STAGE%\LICENSE.txt" >nul
copy /Y "%ROOT%installer\INSTALL.txt" "%STAGE%\INSTALL.txt" >nul
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%' -DestinationPath '%DIST%\PixelLABS-%VERSION%-win64-vst3.zip' -Force"
if errorlevel 1 goto failed

if /I "%~1"=="test" (
    echo Running host test...
    "%BUILD%\test_vst3_host.exe" "%PLUGIN%" "%BUILD%\test-output"
    if errorlevel 1 goto failed
)

echo.
echo Done:
echo   %PLUGIN%
echo   %DIST%\PixelLABS-Setup-%VERSION%.exe
echo   %DIST%\PixelLABS-%VERSION%-win64-vst3.zip
exit /b 0

:no_msvc
echo Could not find the MSVC compiler. Install Visual Studio with "Desktop development with C++",
echo or run this script from an "x64 Native Tools Command Prompt".
exit /b 1

:failed
echo Build failed.
exit /b 1
