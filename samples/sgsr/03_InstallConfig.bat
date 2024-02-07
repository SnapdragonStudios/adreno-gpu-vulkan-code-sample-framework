@echo off

adb push %~dp0/app_config.txt /sdcard/Android/data/com.quic.sgsr/files/app_config.txt

pause
