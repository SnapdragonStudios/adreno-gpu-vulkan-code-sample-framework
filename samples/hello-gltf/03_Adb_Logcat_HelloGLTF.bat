@echo off

@echo Logcat (grep "hellogltf")...
call adb logcat -c
call adb logcat | FIND /I "hellogltf"

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
pause
