@echo off
if EXIST "lib/glslang" goto :compile

echo "Glslang is required to build the shaders."
echo "Download and extract the latest version into lib/glslang and try again."
echo "https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-windows-x64-Release.zip"
goto :end

:compile
setlocal
set PATH=%PATH%;lib/glslang/bin

glslangValidator --target-env vulkan1.2 -V -S rgen shaders/rayGen.glsl -o shaderBin/rayGen.bin
glslangValidator --target-env vulkan1.2 -V -S rchit shaders/rayChit.glsl -o shaderBin/rayChit.bin
glslangValidator --target-env vulkan1.2 -V -S rmiss shaders/rayMiss.glsl -o shaderBin/rayMiss.bin

:end