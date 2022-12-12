@echo off

SETLOCAL
SET CurDir=%~dp0
SET ShaderDir=%CurDir%\shaders
SET Glslang=%ShaderDir%\glslangValidator.exe
SET OutShaderDir=%CurDir%\Media\Shaders

mkdir %CurDir%\Media
rmdir /s /q %OutShaderDir%
mkdir %OutShaderDir%

IF NOT EXIST %Glslang% (
	ECHO "Local GLSL validator is missing - attempting global one"
    SET Glslang=glslangValidator.exe
)

MKDIR %OutShaderDir%

CALL :compile_shader %ShaderDir%\VertexShader.vert vert 0 0 %OutShaderDir%\VertexShader.vert.spv || EXIT /B 1

CALL :compile_shader %ShaderDir%\Downsample.frag frag 0 0 %OutShaderDir%\Downsample.frag.spv || EXIT /B 1
CALL :compile_shader %ShaderDir%\Downsample.frag frag 0 1 %OutShaderDir%\Downsample-Ext.frag.spv || EXIT /B 1

CALL :compile_shader %ShaderDir%\BlurBase.frag frag 0 0 %OutShaderDir%\BlurBase-Horizontal.frag.spv || EXIT /B 1
CALL :compile_shader %ShaderDir%\BlurBase.frag frag 0 1 %OutShaderDir%\BlurBase-Horizontal-Ext.frag.spv || EXIT /B 1

CALL :compile_shader %ShaderDir%\BlurBase.frag frag 1 0 %OutShaderDir%\BlurBase-Vertical.frag.spv || EXIT /B 1
CALL :compile_shader %ShaderDir%\BlurBase.frag frag 1 1 %OutShaderDir%\BlurBase-Vertical-Ext.frag.spv || EXIT /B 1

CALL :compile_shader %ShaderDir%\Display.frag frag 0 0 %OutShaderDir%\Display.frag.spv || EXIT /B 1

EXIT /B 0

:compile_shader
SET ShaderFile=%1
SET Stage=%2
SET VertPass=%3
SET Ext=%4
SET OutName=%5

SET ExtDef=
IF "%Ext%"=="1" (
    SET ExtDef="-DENABLE_QCOM_IMAGE_PROCESSING=1"
)
SET VertDef=
IF "%VertPass%"=="1" (
    SET VertDef="-DVERT_PASS=1"
)

CALL %Glslang% -V -S %Stage% %ExtDef% %VertDef% -e main -o "%OutName%" "%ShaderFile%" || EXIT /B 1

EXIT /B 0
