@echo off
cd /D "%~dp0"
mkdir solutionArm64
pushd solutionArm64
echo Looking for Visual Studio...
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version [17.0,18.0] -format value -property displayName |findstr 2022
if %ERRORLEVEL%==0 goto :FOUND2022
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version [16.0,17.0] -format value -property displayName |findstr 2019
if %ERRORLEVEL%==0 goto :FOUND2019
echo Not found VS2019 or VS2022 using 'vswhere'.  Attempting to create VS2019 solution anyways.

:FOUND2019
echo Creating Visual Studio 2019, ARM64 solution
echo.
echo.
cmake.exe -G "Visual Studio 16 2019" -A "ARM64" ..
if %ERRORLEVEL% ==0  goto :BUILD
popd
echo.
echo Could not build Visual Studio 2019 .sln files.  Check above errors (Visual Studio Pro 2022 or 2019 supported)
goto :EOF

:FOUND2022
echo Creating Visual Studio 2022, ARM64 solution
echo.
echo.
cmake.exe -G "Visual Studio 17 2022" -A "ARM64" ..
if %ERRORLEVEL% ==0  goto :BUILD
echo Could not build Visual Studio 2022 .sln files.  Check above errors.
popd
goto :EOF

:BUILD
echo Visual Studio solution created in project\windows\solutionArm64\
echo Starting debug build.
echo .
cmake.exe --build . --config Debug
echo Starting release build.
echo .
cmake.exe --build . --config Release
popd

echo.
echo Visual Studio solution written to %CD%\solutionArm64\SampleFramework.sln
echo.

