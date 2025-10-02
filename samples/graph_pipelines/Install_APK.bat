@echo off
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\graph_pipelines\outputs\apk\debug\graph_pipelines-debug.apk
@echo ****************************************
call adb install -r -t ..\..\build\android\graph_pipelines\outputs\apk\debug\graph_pipelines-debug.apk
@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
