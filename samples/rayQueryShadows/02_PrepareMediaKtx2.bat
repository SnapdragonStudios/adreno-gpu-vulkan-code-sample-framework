rem @echo off
setlocal

echo.
echo Copying from vkSampleFrameworkAssets/shared/Media (shared Assets submodule)
echo.
mkdir Media
rmdir /s /q Media\Objects
mkdir Media\Objects
rmdir /s /q Media\Textures
mkdir Media\Textures

xcopy ..\..\vkSampleFrameworkAssets\shared\Media\Textures\white_d.ktx Media\Textures\.
xcopy ..\..\vkSampleFrameworkAssets\shared\Media\Textures\default_ddn.ktx Media\Textures\.
xcopy ..\..\vkSampleFrameworkAssets\shared\Media\Textures\simplesky_env.ktx Media\Textures\.
xcopy ..\..\vkSampleFrameworkAssets\shared\Media\Textures\simplesky_irradiance.ktx Media\Textures\.

xcopy /s /y /k /i ..\..\vkSampleFrameworkAssets\shared\Media\Objects\Skybox_Separate.* Media\Objects\.

rem xcopy /s /y /k /i ..\..\vkSampleFrameworkAssets\shared\Media\Objects\BistroGltfFused Media\Objects\BistroGltfFused
rem call :ConvertTextures Media\Objects\BistroGltfFused 2048

xcopy /s /y /k /i ..\..\vkSampleFrameworkAssets\shared\Media\Objects\BistroGltfExterior Media\Objects\BistroExteriorGltf
call :ConvertTextures Media\Objects\BistroExteriorGltf 2048

echo.
echo Cleaning up afer texture conversion
echo.
pushd Media
del /s /q /f *.png *.jpg *.tga *.dds
popd

goto:eof


:ConvertTextures
rem %1 is directory, %2 is size at which we downsample (in bytes)
rem Convert png images to ktx textures.
rem Larger textures are mipped and reduced in size, (very) small textures are not
SET SIZELIMIT=1000
pushd %1
for /R %%i in (*.png) do (
    echo %%i|findstr /i /L "Normal">nul
    if errorlevel 1 (
        call :ConvertTexture %%i %2
    ) else ( 
        rem Normal Texture (adjust compression for 'normal node' but keep the 3 components rather than compressing to 2 components which needs a shader change!)
        call :ConvertTexture %%i %2 --input_swizzle rgb1 --normal_mode --assign_oetf linear
    )
)
for /R %%i in (*.dds) do (
    echo %%i|findstr /i /L "Normal">nul
    if errorlevel 1 (
        call :ConvertTexture %%i %2
    ) else ( 
        rem Normal Texture (adjust compression for 'normal node' but keep the 3 components rather than compressing to 2 components which needs a shader change!)
        call :ConvertTexture %%i %2 --input_swizzle rgb1 --normal_mode --assign_oetf linear
    )
)
popd
goto :eof

:ConvertTexture
rem %1 is file, %2 is size at which we downsample (in bytes), %3... are additional options (eg for normal maps) passed to the converter
rem Convert png image to ktx texture.
if %~z1 LSS %2 (
    echo "Not scaling (or mipping) %~f1 (too small)"
    ..\..\..\..\..\project\tools\toktx.exe --encode etc1s %3 %4 %5 %6 %7 %8 %9 "%~dpn1.ktx" "%~f1"
) else (
    ..\..\..\..\..\project\tools\toktx.exe --encode etc1s --genmipmap %3 %4 %5 %6 %7 %8 %9 "%~dpn1.ktx" "%~f1" 
)
goto :eof
