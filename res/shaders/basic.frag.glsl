#version 460

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_texcoord;

layout(location = 0) out vec4 out_color;

layout(set=2, binding=0) uniform sampler2D image;

void main() {
    out_color = texture(image, in_texcoord) * in_color;
}
