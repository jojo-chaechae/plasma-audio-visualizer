#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 v_uv;

layout(std140, binding = 0) uniform buf {
    mat4 uClip;
    vec4 p0;
    vec4 p1;
    vec4 p2;
    vec4 p3;
    vec4 p4;
    vec4 p5;
};

void main()
{
    v_uv = uv;
    gl_Position = uClip * vec4(position, 0.0, 1.0);
}
