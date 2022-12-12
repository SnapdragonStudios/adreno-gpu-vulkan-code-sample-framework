@echo off
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\hdrSwapchain\outputs\apk\debug\hdrSwapchain-debug.apk
@echo ****************************************
call adb install -r -t ..\..\build\android\hdrSwapchain\outputs\apk\debug\hdrSwapchain-debug.apk
@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
