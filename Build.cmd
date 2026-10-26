@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if not errorlevel 1 goto tools_ready
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CD_VS=%%i"
if not defined CD_VS (
  echo Visual Studio C++ tools are required.
  exit /b 1
)
call "%CD_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if not "%errorlevel%"=="0" exit /b %errorlevel%
set "PATH=%CD_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%CD_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
:tools_ready
cmake --preset windows-dev
if not "%errorlevel%"=="0" exit /b %errorlevel%
cmake --build --preset windows-dev
if not "%errorlevel%"=="0" exit /b %errorlevel%
ctest --preset windows-dev
exit /b %errorlevel%
