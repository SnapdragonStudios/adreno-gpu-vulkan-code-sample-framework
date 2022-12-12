@echo off
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\hello-gltf\outputs\apk\debug\hello-gltf-debug.apk
@echo ****************************************
call adb install -r -t ..\..\build\android\hello-gltf\outputs\apk\debug\hello-gltf-debug.apk
@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
