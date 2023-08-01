@echo off
setlocal

echo.
echo Copying from vkSampleFrameworkAssets/shared/Media (shared Assets submodule)
echo.
mkdir Media
rmdir /s /q Media\Objects
mkdir Media\Objects
rmdir /s /q Media\Textures
mkdir Media\Textures

xcopy /s /y /k /i ..\..\vkSampleFrameworkAssets\shared\Media\Objects\Skull_Separate.* Media\Objects\.

xcopy /s /y /k /i ..\..\vkSampleFrameworkAssets\shared\Media\Textures\white_d.ktx Media\Textures\.

..\..\project\tools\simpletextureconverter ..\..\vkSampleFrameworkAssets\shared\Media\Textures\Skull_Diffuse.jpg .\Media\Textures\Skull_Diffuse.ktx -format R8G8B8A8Unorm
