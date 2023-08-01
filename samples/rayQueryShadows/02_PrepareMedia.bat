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
        call :ConvertTexture %%i %2 R8G8B8A8UnormSrgb
    ) else ( 
        rem Normal Texture (output linear 'color' values)
        call :ConvertTexture %%i %2 R8G8B8A8Unorm
    )
)
for /R %%i in (*.dds) do (
    echo %%i|findstr /i /L "Normal">nul
    if errorlevel 1 (
        call :ConvertTexture %%i %2 R8G8B8A8UnormSrgb
    ) else ( 
        rem Normal Texture (output linear 'color' values)
        call :ConvertTexture %%i %2 R8G8B8A8Unorm
    )
)
popd
goto :eof

:ConvertTexture
rem %1 is file, %2 is size at which we downsample (in bytes), %3 is the output data format
rem Convert png image to ktx texture.
if %~z1 LSS %2 (
    echo "Not scaling (or mipping) %~f1 (too small)"
    ..\..\..\..\..\project\tools\simpletextureconverter.exe "%~f1" "%~dpn1.ktx" -F %3 -nomip
) else (
    ..\..\..\..\..\project\tools\simpletextureconverter.exe "%~f1" "%~dpn1.ktx" -F %3 -w 25%% -h 25%%
)
goto :eof
