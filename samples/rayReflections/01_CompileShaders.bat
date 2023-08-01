@echo off

mkdir .\Media\Shaders

@echo.
echo ****************************************
echo Compiling Shaders...
echo ****************************************
for %%i in (shaders\*.vert) do (
    call :COMPILE %%i || GOTO COMPILE_FAILED
)
for %%i in (shaders\*.frag) do (
    call :COMPILE %%i || GOTO COMPILE_FAILED
)
for %%i in (shaders\*.comp) do (
    call :COMPILE %%i || GOTO COMPILE_FAILED
)
for %%i in (shaders\*.rgen shaders\*.rint shaders\*.rahit shaders\*.rchit shaders\*.rmiss shaders\*.rcall) do (
    call :COMPILE %%i || GOTO COMPILE_FAILED
)

@echo.
echo ****************************************
echo Copying .json
echo ****************************************
xcopy /y shaders\*.json .\Media\Shaders\.

@echo.
echo ****************************************
echo Done
echo ****************************************
IF %0 EQU "%~dpnx0" PAUSE
goto :EOF

:COMPILE
glslangValidator.exe -V --target-env spirv1.4 %1 -o .\Media\Shaders\%~nx1.spv
IF NOT ERRORLEVEL 1 echo. %1 -^> .\Media\Shaders\%~nx1.spv
goto :EOF

:COMPILE_FAILED
echo COMPILE FAILED
IF %0 EQU "%~dpnx0" PAUSE
