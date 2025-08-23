#version 460

void main() {
    vec4 vertices[] = {
        vec4(-0.5, -0.5, 0.0, 1.0),
        vec4( 0.5, -0.5, 0.0, 1.0),
        vec4( 0.0,  0.5, 0.0, 1.0),
    };

    gl_Position = vertices[gl_VertexIndex];
}
