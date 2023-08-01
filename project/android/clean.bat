@echo off
pushd .
echo Running gradlew clean build
call gradlew clean --no-daemon
cd ..\..
echo Removing all .externalNativeBuild directories.
for /d /r . %%d in (.externalNativeBuild) do @if exist "%%d" rd /s/q "%%d"
for /d /r . %%d in (.cxx) do @if exist "%%d" rd /s/q "%%d"
popd
exit /b
