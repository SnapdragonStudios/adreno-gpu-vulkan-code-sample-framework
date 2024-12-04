@echo off
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\sgsr2\outputs\apk\debug\sgsr2-debug.apk
@echo ****************************************
call adb install -r -t ..\..\build\android\sgsr2\outputs\apk\debug\sgsr2-debug.apk
@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
