@echo off
pushd .
cd /D "%~dp0"

adb uninstall com.quic.BloomImageProcessing

@echo.
@echo ****************************************
@echo Install ..\..\build\android\BloomImageProcessing\outputs\apk\debug\BloomImageProcessing-debug.apk
@echo ****************************************
adb install -r -g ..\..\build\android\BloomImageProcessing\outputs\apk\debug\BloomImageProcessing-debug.apk

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
popd
IF %0 EQU "%~dpnx0" PAUSE
