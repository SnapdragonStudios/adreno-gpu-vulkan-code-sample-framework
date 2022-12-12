@echo off
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\SubPass\outputs\apk\debug\SubPass-debug.apk
@echo ****************************************
call adb install -r -t ..\..\build\android\SubPass\outputs\apk\debug\SubPass-debug.apk
@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
