#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 position;

out float illumination_amount;

uniform mat4 mvp_transform;
uniform float light_attenuation;
uniform float light_intensity;

void main() {
    vec4 world_position = mvp_transform*vec4(position, 1.0f);
    illumination_amount = log(1.0f/(length(world_position)*light_attenuation))*light_intensity;
    gl_Position = world_position;
}


#shader GL_FRAGMENT_SHADER
#version 330

in float illumination_amount;

out vec4 frag_color;

uniform vec4 color;

void main() {
    frag_color = color*illumination_amount;
}
