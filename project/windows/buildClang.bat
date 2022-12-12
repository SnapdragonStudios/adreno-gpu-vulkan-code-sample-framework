@mkdir solutionClang
@pushd solutionClang
cmake.exe -G "Visual Studio 16 2019" -T "ClangCL" ..
cmake.exe --build . --config Debug
cmake.exe --build . --config Release
@popd
