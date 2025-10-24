pushd .
cd /D %~dp0
rmdir /s /q solution
rmdir /s /q ..\..\samples\external\.fetchcontent
rmdir /s /q ..\..\samples\fidelityFx\external\FidelityFX-FSR2\build
popd

