#!/bin/sh
if ! [ -d "lib/glslang" ];  then
  echo "Downloading glslang"
  wget https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-linux-Release.zip -O lib/glslang-master-linux-Release.zip
  unzip lib/glslang-master-linux-Release.zip -d lib/glslang
  rm lib/glslang-master-linux-Release.zip
fi

PATH=$PATH:lib/glslang/bin

glslangValidator --target-env vulkan1.2 -V -S rgen shaders/rayGen.glsl -o shaderBin/rayGen.bin
glslangValidator --target-env vulkan1.2 -V -S rchit shaders/rayChit.glsl -o shaderBin/rayChit.bin
glslangValidator --target-env vulkan1.2 -V -S rmiss shaders/rayMiss.glsl -o shaderBin/rayMiss.bin