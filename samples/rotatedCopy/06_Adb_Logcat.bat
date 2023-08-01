@echo off

@echo Logcat...
call adb logcat -c
call adb logcat

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
pause