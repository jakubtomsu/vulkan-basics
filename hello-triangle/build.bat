@echo off

set vk_path=d:/VulkanSDK/1.2.182.0
set shader_compiler=glslc

echo build shaders...
%shader_compiler% shader.vert -o shader.vert.spv
%shader_compiler% shader.frag -o shader.frag.spv

echo build c...
cl /Iglfw_include /I%vk_path%/Include main.c /link /LIBPATH:glfw_lib_vc2019 /LIBPATH:%vk_path%/Lib