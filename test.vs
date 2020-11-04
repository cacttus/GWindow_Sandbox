#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 _v301;
layout(location = 1) in vec4 _c401;
layout(location = 2) in vec2 _x201;
layout(location = 3) in vec3 _n301;

layout(location = 0) out vec4 _vColorVS;
layout(location = 1) out vec2 _vTexcoordVS;
layout(location = 2) out vec3 _vNormalVS;
layout(location = 3) out vec3 _vPositionVS;
layout(location = 4) out vec3 _vCamPosVS;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 camPos;
    float pad;
} _uboViewProj;

struct InstanceData {
    mat4 model;
};
//An array of UBO blocks isn't supported.
layout (binding = 1) uniform Instances {
  InstanceData instances[1000]; // This is a member
} _uboInstanceData;

void main() {
  //gl_InstanceID
  //gl_InstanceIndex
  mat4 m_model = _uboInstanceData.instances[gl_InstanceIndex].model;

  vec4 p_t = m_model * vec4(_v301,1);
  gl_Position = _uboViewProj.proj * _uboViewProj.view * p_t;
  _vColorVS = _c401;
  _vTexcoordVS = _x201;
  _vPositionVS = p_t.xyz;
  _vNormalVS = normalize( (transpose(inverse(m_model * _uboViewProj.view)) * vec4(_n301,1)).xyz );//Timv
  _vCamPosVS = _uboViewProj.camPos;
}
