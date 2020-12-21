#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec3 normal;

out Fragment_Data {
    out vec3 position;
    out vec3 world_position;
    out vec2 texcoord;
    out vec3 normal;
} fragment;

uniform mat4 model_transform;
uniform mat4 mvp_transform;

void main() {
    fragment.position = position;
    fragment.world_position = vec3(model_transform * vec4(position, 1.0f));
    fragment.texcoord = texcoord;
    fragment.normal   = normal;
        
    gl_Position = mvp_transform * vec4(position, 1.0f);
}


#shader GL_FRAGMENT_SHADER
#version 330

in Fragment_Data {
    vec3 position;
    vec3 world_position;
    vec2 texcoord;
    vec3 normal;
} fragment;

out vec4 frag_color;

struct Light_Setup {
    vec3 position;
    vec3 view_position;
    vec3 color;
    float ambient_intensity;
};

uniform float fog_density;
uniform float fog_gradient;

const float specular_intensity = 0.5f;

uniform Light_Setup light_setup;
uniform sampler2D sampler;
uniform vec4 object_color;
uniform vec3 sky_color;

vec4 apply_fog(in vec4 color) {
    float dist = length(-light_setup.view_position - fragment.position);
    float fog_amount = exp(-pow(dist * fog_density, fog_gradient));
    fog_amount = clamp(fog_amount, 0.0f, 1.0f);
    return mix(vec4(sky_color, 1.0f), color, fog_amount);
}

void main() {
    // Texture color
    vec4 texel_color = texture2D(sampler, fragment.texcoord);

    // Calculate phong lighting
    vec3 light_direction = normalize(light_setup.position - fragment.world_position);
    vec3 view_direction = normalize(light_setup.view_position - fragment.world_position);
    vec3 reflect_direction = reflect(-light_direction, fragment.normal);
    
    float diffuse_amount = max(dot(fragment.normal, light_direction), 0.0f);
    float specular_amount = pow(max(dot(view_direction, reflect_direction), 0.0f), 32);

    vec4 ambient  = vec4(light_setup.ambient_intensity * light_setup.color, 1.0f);
    vec4 diffuse  = vec4(diffuse_amount * light_setup.color, 1.0f);
    vec4 specular = vec4(specular_intensity * specular_amount * light_setup.color, 1.0f);

    // Calculate the final fragment color
    frag_color = (ambient + diffuse + specular) * texel_color * object_color;

    // Apply some fog
    frag_color = apply_fog(frag_color);
}
