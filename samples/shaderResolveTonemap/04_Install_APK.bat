@echo off
pushd .
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\shaderResolveTonemap\outputs\apk\debug\shaderResolveTonemap-debug.apk
@echo ****************************************
adb install -r ..\..\build\android\shaderResolveTonemap\outputs\apk\debug\shaderResolveTonemap-debug.apk

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
popd
IF %0 EQU "%~dpnx0" PAUSE
