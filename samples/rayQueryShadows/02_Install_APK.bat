@echo off
pushd .
cd /D "%~dp0"

@echo.
@echo ****************************************
@echo Install ..\..\build\android\rayQueryShadows\outputs\apk\debug\rayQueryShadows-debug.apk
@echo ****************************************
adb install -r ..\..\build\android\rayQueryShadows\outputs\apk\debug\rayQueryShadows-debug.apk

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
popd
IF %0 EQU "%~dpnx0" PAUSE
