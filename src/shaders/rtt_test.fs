#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 _outFBOColor;
//layout(location = 1) out float _outFBODepth;
 
//layout(location = 0) out vec4 _gPositionOut; 
//layout(location = 1) out vec4 _gColorOut;
//layout(location = 2) out vec4 _gNormalOut;
//layout(location = 3) out vec4 _gPlaneOut;
//layout(location = 4) out uint _gPickOut;

//A combined image sampler descriptor is represented in GLSL by a sampler uniform
layout(binding = 2) uniform sampler2D _ufTexture0;

void main() {  
    _outFBOColor = vec4(fragColor.rgb, fragColor.a) * texture(_ufTexture0, fragTexCoord);
} 
