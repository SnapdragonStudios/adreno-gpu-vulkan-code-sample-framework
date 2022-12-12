@echo off
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\shaderResolveTonemap\outputs\apk\debug\shaderResolveTonemap-debug.apk
@echo ****************************************
call adb install -r -t ..\..\build\android\shaderResolveTonemap\outputs\apk\debug\shaderResolveTonemap-debug.apk
@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
