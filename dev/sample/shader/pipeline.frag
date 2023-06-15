#version 450

layout(binding = 1) uniform UniformBuffer1 { vec3 color; }
u;

layout(location = 0) out vec4 o_color;

void main() { o_color = vec4(u.color, 1.0); }
