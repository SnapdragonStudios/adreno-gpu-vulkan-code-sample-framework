@pushd ..\..
@cd framework\external\KTX-Software
@git fetch origin 'refs/tags/*:refs/tags/*'
@popd
@mkdir solution
@pushd solution
cmake.exe -G "Visual Studio 16 2019" ..
cmake.exe --build . --config Debug
cmake.exe --build . --config Release
@popd
