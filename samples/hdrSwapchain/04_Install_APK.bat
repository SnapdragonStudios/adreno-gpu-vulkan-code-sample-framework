@echo off
pushd .
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\hdrSwapchain\outputs\apk\debug\hdrSwapchain-debug.apk
@echo ****************************************
adb install -r ..\..\build\android\hdrSwapchain\outputs\apk\debug\hdrSwapchain-debug.apk

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
popd
IF %0 EQU "%~dpnx0" PAUSE
