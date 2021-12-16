@mkdir solution
@pushd solution
cmake.exe -G "Visual Studio 16 2019" ..
@echo Replacing build platform version with 10.0 so generated vcxproj files do not target a specific plaform version.
@rem there is literally no way to do this in CMake3.17, will always want to specify one of the versions on the machine generating the vcxproj files.
@powershell -Command "gci . *.vcxproj -recurse | Where-Object {$_.PSParentPath -notmatch \"CMakeFiles\"} | ForEach {(Get-Content $_.FullName | ForEach {$_ -replace \"WindowsTargetPlatformVersion.*^<\", \"WindowsTargetPlatformVersion^>10.0^<\"}) | Set-Content $_.Fullname"}
cmake.exe --build . --config Debug
cmake.exe --build . --config Release
@popd
