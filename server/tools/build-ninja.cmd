@echo off
rem Builds the server with the Ninja generator + MSVC, from any shell.
rem The Ninja generator needs cl.exe and the VS dev environment (INCLUDE/LIB),
rem so we source vcvarsall first. Usage:  tools\build-ninja.cmd [--clean]
setlocal

rem Locate VS 2022 via vswhere (fallback to the known install path)
set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VSINSTALL=%%i"
if not defined VSINSTALL set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Community"

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 (
    echo Failed to initialize the VS 2022 x64 dev environment from "%VSINSTALL%" 1>&2
    exit /b 1
)

rem vcvarsall points VCPKG_ROOT at the VS-bundled vcpkg; force the project one
rem (C:\vcpkg) and make sure ninja (chocolatey) is on PATH.
set "VCPKG_ROOT=C:\vcpkg"
set "PATH=C:\vcpkg;C:\ProgramData\chocolatey\bin;%PATH%"

cd /d "%~dp0.."

if /i "%~1"=="--clean" rmdir /s /q build-ninja 2>nul

cmake --preset windows-ninja || exit /b 1
cmake --build --preset windows-ninja || exit /b 1

echo.
echo Built: %cd%\build-ninja\bin\ledashboard_server.exe
exit /b 0
