#version 450

void main() {
    // Vertex positions (in Y pointing down manner)
    const float scale       = 0.3f;
    vec2        vertices[3] = vec2[](vec2(-1.0, 1.0) * scale, vec2(3.0, 1.0) * scale, vec2(-1.0, -3.0) * scale);

    // Calculate the vertex position using the vertex ID
    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
}
