#version 450

layout(location = 0) in vec2 i_position;

layout(binding = 0) uniform UniformBuffer0 { vec2 offset; }
u;

void main() { gl_Position = vec4(i_position + u.offset, 0.0, 1.0); }
