@echo off

@echo Logcat (grep "graph_pipelines")...
call adb logcat -c
call adb logcat | FIND /I "graph_pipelines"

@echo.
@echo ****************************************
@echo Done!
@echo ****************************************
pause
