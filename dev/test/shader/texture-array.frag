#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform Uniform {
    vec2 texCoord;
    int  texIndex;
}
u;
layout(binding = 1) uniform texture2D u_textures[32];
layout(binding = 2) uniform sampler u_sampler;
layout(location = 0) out vec4 o_color;

void main() { o_color = texture(sampler2D(u_textures[u.texIndex], u_sampler), u.texCoord); }