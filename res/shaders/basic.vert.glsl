#version 460

layout(set = 1, binding = 0) uniform Uniform_Block {
    mat4 mvp; // Model-view-projection matrix
};

void main() {
    vec4 vertices[] = {
        vec4(-0.5, -0.5, 0.0, 1.0),
        vec4( 0.5, -0.5, 0.0, 1.0),
        vec4( 0.0,  0.5, 0.0, 1.0),
    };

    gl_Position = mvp * vertices[gl_VertexIndex];
}
