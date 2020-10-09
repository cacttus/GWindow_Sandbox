#version 450
#extension GL_ARB_separate_shader_objects : enable

//note dvec3 uses multiple slots.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;


float a=-1.0, b=1.0;
//clip space in the example.
//  -1, -1
//
//            1, 1
vec2 positions[9] = vec2[](
  //2 triangles for background, 1 for foreground
    vec2(a, a),
    vec2(b, b),
    vec2(a, b ),

    vec2(a, a),
    vec2(b, a),
    vec2(b, b),

    //FG triangle
    vec2(0.0, -.5),
    vec2(.5, .5),
    vec2(-.5, .5)
);

float ca=0.1;
float cb=0.7;

vec4 colors[9] = vec4[](
    vec4(ca, ca, ca, 1),
    vec4(cb, cb, cb, 1),
    vec4(ca, ca, ca, 1),
        
    vec4(ca, ca, ca, 1),
    vec4(cb, cb, cb, 1),
    vec4(cb, cb, cb, 1),

    vec4(1.0, 0.0, 0.0, 1 ),
    vec4(0.0, 1.0, 0.0, 1 ),
    vec4(0.0, 0.0, 1.0, 0.0) //alpha 
);

void main() {
    //**TODO: finish implementing vertex buffers , add PVM matrices.

    //gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    //fragColor = colors[gl_VertexIndex];

    gl_Position = ubo.proj * ubo.view * ubo.model *vec4(inPosition,1);
    fragColor = inColor;
}
