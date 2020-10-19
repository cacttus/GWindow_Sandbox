#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

//A combined image sampler descriptor is represented in GLSL by a sampler uniform
layout(binding = 2) uniform sampler2D _ufTexture0;

void main() {  
    outColor = vec4(fragColor.rgb, fragColor.a) * texture(_ufTexture0, fragTexCoord);
} 
