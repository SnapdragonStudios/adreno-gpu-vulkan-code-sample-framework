@del rayQueryShadows.zip
copy /y ..\..\project\windows\solution\samples\rayQueryShadows\Debug\rayQueryShadows.exe rayQueryShadows_debug.exe
copy /y ..\..\project\windows\solution\samples\rayQueryShadows\Release\rayQueryShadows.exe rayQueryShadows_release.exe
copy /y ..\..\project\windows\solutionArm64\samples\rayQueryShadows\Debug\rayQueryShadows.exe rayQueryShadows_arm64_debug.exe
copy /y ..\..\project\windows\solutionArm64\samples\rayQueryShadows\Release\rayQueryShadows.exe rayQueryShadows_arm64_release.exe
zip -r rayQueryShadows.zip Media rayQueryShadows_debug.exe rayQueryShadows_release.exe rayQueryShadows_arm64_debug.exe rayQueryShadows_arm64_release.exe
del /q rayQueryShadows_debug.exe rayQueryShadows_release.exe rayQueryShadows_arm64_debug.exe rayQueryShadows_arm64_release.exe

