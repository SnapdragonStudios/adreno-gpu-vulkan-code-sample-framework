@echo off
pushd .
cd /D "%~dp0"

adb uninstall com.quic.sgsr

@echo.
@echo ****************************************
@echo Install ..\..\build\android\sgsr\outputs\apk\debug\sgsr-debug.apk
@echo ****************************************
adb install -r ..\..\build\android\sgsr\outputs\apk\debug\sgsr-debug.apk

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
popd
IF %0 EQU "%~dpnx0" PAUSE
