#version 450
#extension GL_ARB_separate_shader_objects : enable

//I wonder if you can do multiple vertex bindings.
//note dvec3 uses multiple slots.
//SPV-Reflect puts these in order (by location) for us
layout(location = 0) in vec3 _v301;
layout(location = 1) in vec4 _c401;
layout(location = 2) in vec2 _x201;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0) uniform UniformBufferObject {
//Note: Must be aligned to std120
    mat4 view;
    mat4 proj;
} _uboViewProj;//ubo is the name.


struct InstanceData
{
    mat4 model;
};
layout (binding = 1) uniform Instances {
  InstanceData instances[100];
} _uboInstanceData;

// struct Blah {
//   vec4 testing_1;
//   vec4 testing_2;
// };
// //GLSL-V only allows arrays of opauqe types
// layout(binding = 3) uniform sampler2D test_samplers[5];


void main() {
  //gl_InstanceID
  //gl_InstanceIndex
  gl_Position = _uboViewProj.proj * _uboViewProj.view * _uboInstanceData.instances[gl_InstanceIndex].model * vec4(_v301,1);
  fragColor = _c401;
  fragTexCoord = _x201;
}
