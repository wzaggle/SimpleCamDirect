@echo off
setlocal
pushd "%~dp0"
if not exist SonyUdpProbe.exe goto :NotBuilt
set "CameraIP=192.168.0.100"
set /p "CameraIP=Camera IPv4 address [192.168.0.100]: "
set "CameraPort=52381"
set /p "CameraPort=UDP port [52381]: "
set "LocalPort=52381"
set /p "LocalPort=PC listening UDP port [52381]: "
set "TestMode=zoom-test"
set /p "TestMode=Test mode - zoom-test, spaced-zoom, power-seq2, compare-seq2, compare, positions or identity [zoom-test]: "
SonyUdpProbe.exe "%CameraIP%" "%CameraPort%" 3000 "%LocalPort%" 1 "%TestMode%"
set "ProbeResult=%errorlevel%"
echo.
echo Attach the SonyUdpProbe-*.log file from this folder with your result.
popd
pause
exit /b %ProbeResult%
:NotBuilt
echo First double-click Build-Windows.cmd. Then run this launcher again.
popd
pause
exit /b 1
