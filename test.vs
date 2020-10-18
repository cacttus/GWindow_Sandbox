#version 450
#extension GL_ARB_separate_shader_objects : enable

//I wonder if you can do multiple vertex bindings.
//note dvec3 uses multiple slots.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0) uniform UniformBufferObject {
//Note: Must be aligned to std120
    mat4 view;
    mat4 proj;
} ubo;

struct InstanceData
{
    mat4 model;
};
layout (binding = 1) uniform Instances {
  InstanceData instances[100];
};


void main() {
  //gl_InstanceID
  //gl_InstanceIndex
  gl_Position = ubo.proj * ubo.view * instances[gl_InstanceIndex].model * vec4(inPosition,1);
  fragColor = inColor;
  fragTexCoord = inTexCoord;
}
