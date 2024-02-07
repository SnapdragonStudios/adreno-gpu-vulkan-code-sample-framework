@echo off

adb push %~dp0/app_config.txt /sdcard/Android/data/com.quic.BloomImageProcessing/files/app_config.txt

pause
