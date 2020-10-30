#version 450
#extension GL_ARB_separate_shader_objects : enable

//I wonder if you can do multiple vertex bindings.
//note dvec3 uses multiple slots.
//SPV-Reflect puts these in order (by location) for us
layout(location = 0) in vec3 _v301;
layout(location = 1) in vec4 _c401;
layout(location = 2) in vec2 _x201;
layout(location = 3) in vec3 _n301;

layout(location = 0) out vec4 _vColorVS;
layout(location = 1) out vec2 _vTexcoordVS;
layout(location = 2) out vec3 _vNormalVS;
layout(location = 3) out vec3 _vPositionVS;

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
  InstanceData instances[1000]; // This is a member
} _uboInstanceData;

void main() {
  //gl_InstanceID
  //gl_InstanceIndex
  mat4 m_model = _uboInstanceData.instances[gl_InstanceIndex].model;

  vec4 p_t = m_model * vec4(_v301,1);
  vec4 n_t = m_model * vec4(_n301,1);
  gl_Position = _uboViewProj.proj * _uboViewProj.view * p_t;
  _vColorVS = _c401;
  _vTexcoordVS = _x201;
  _vNormalVS = normalize(n_t.xyz);
  _vPositionVS = p_t.xyz;
  _vNormalVS = normalize((m_model * vec4(_v301 + _n301, 1)).xyz - _vPositionVS);


}
