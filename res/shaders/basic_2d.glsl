#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec2 pos;
layout(location=1) in vec4 color;

out vec4 out_color;

uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(pos, 0.0f, 1.0f);
    out_color = color;
}


#shader GL_FRAGMENT_SHADER
#version 330

in vec4 out_color;

out vec4 frag_color;

uniform vec4 color;

void main() {
    frag_color = out_color*color;
}

