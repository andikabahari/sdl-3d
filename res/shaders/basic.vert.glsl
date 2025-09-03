#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_texcoord;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_texcoord;

layout(set=1, binding=0) uniform Uniform_Block {
    mat4 mvp; // Model-view-projection matrix
};

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    out_color = in_color;
    out_texcoord = in_texcoord;
}
