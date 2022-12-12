@echo off
pushd .
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\rotatedCopy\outputs\apk\debug\rotatedCopy-debug.apk
@echo ****************************************
adb install -r ..\..\build\android\rotatedCopy\outputs\apk\debug\rotatedCopy-debug.apk

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
popd
IF %0 EQU "%~dpnx0" PAUSE
