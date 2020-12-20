#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 positions;
layout(location=1) in vec2 texcoords;
layout(location=2) in vec3 normals;

out vec2 texcoords0;

uniform mat4 mvp_transform;

void main() {
    vec4 world_position = mvp_transform * vec4(positions, 1.0f);
    texcoords0 = texcoords;
    gl_Position = world_position;
}


#shader GL_FRAGMENT_SHADER
#version 330

in vec2 texcoords0;

out vec4 frag_color;

struct Light_Setup {
    vec3 color;
    float ambient_intensity;
};

uniform Light_Setup light_setup;
uniform sampler2D sampler;
uniform vec4 object_color;

void main() {
    vec4 texel_color = texture2D(sampler, texcoords0);
    vec4 ambient_light_color = vec4(light_setup.color * light_setup.ambient_intensity, 1.0f);
    frag_color = texel_color * ambient_light_color * object_color;
}
