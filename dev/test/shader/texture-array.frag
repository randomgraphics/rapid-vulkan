#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform Uniform {
    vec2 texCoord;
    int  texIndex;
} u;
layout(binding = 1) uniform sampler2D u_textures[];
layout(location = 0) out vec4 o_color;

void main() { o_color = texture(u_textures[u.texIndex], u.texCoord); }