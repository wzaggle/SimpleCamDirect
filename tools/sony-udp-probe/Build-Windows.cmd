@echo off
setlocal
pushd "%~dp0"
where cl.exe >nul 2>&1
if errorlevel 1 call :FindCompiler
where cl.exe >nul 2>&1
if errorlevel 1 goto :NoCompiler
cl.exe /nologo /std:c++17 /EHsc /W4 /O2 /MT /D_WIN32_WINNT=0x0601 sony_udp_probe.cpp /Fe:SonyUdpProbe.exe /link ws2_32.lib
set "BuildResult=%errorlevel%"
if not "%BuildResult%"=="0" goto :BuildFailed
echo.
echo Build complete. Double-click Run-Probe.cmd to test the camera.
popd
pause
exit /b 0
:FindCompiler
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT exit /b 1
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
exit /b
:NoCompiler
echo Visual Studio C++ compiler not found. Install or enable Desktop development with C++.
popd
pause
exit /b 1
:BuildFailed
echo Build failed. Please send the compiler output.
popd
pause
exit /b %BuildResult%
