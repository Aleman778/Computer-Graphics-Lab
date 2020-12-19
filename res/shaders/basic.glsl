#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 pos;
out float illumination_amount;
uniform mat4 transform;
uniform float light_attenuation;
uniform float light_intensity;

void main() {
    vec4 world_pos = transform * vec4(pos, 1.0f);
    illumination_amount = log(1.0f/(length(world_pos)*light_attenuation))*light_intensity;
    gl_Position = world_pos;
}

#shader GL_FRAGMENT_SHADER
#version 330

out vec4 frag_color;
in float illumination_amount;
uniform vec4 color;

void main() {
    frag_color = color*illumination_amount;
}

