#version 450

layout(location = 0) in vec2 i_position;

/// A passthrough vertex shader for 2D vertex positions
void main() {
    // Vertex positions (in Y pointing down manner)
    vec2 vertices[3] = vec2[](vec2(-1.0, 1.0), vec2(3.0, 1.0), vec2(-1.0, -3.0));

    // Calculate the vertex position using the vertex ID
    gl_Position = vec4(i_position, 0.0, 1.0);
}
