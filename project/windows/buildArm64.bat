@mkdir solutionArm64
@pushd solutionArm64
cmake.exe -G "Visual Studio 16 2019" -A "ARM64" ..
cmake.exe --build . --config Debug
cmake.exe --build . --config Release
@popd
