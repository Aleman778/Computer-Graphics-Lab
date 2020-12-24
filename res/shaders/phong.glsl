
/***************************************************************************
 * Vertex Shader
 ***************************************************************************/

#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec3 normal;

out Fragment_Data {
    vec3 position;
    vec3 world_position;
    vec2 texcoord;
    vec3 normal;
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

/***************************************************************************
 * Fragment Shader
 ***************************************************************************/

#shader GL_FRAGMENT_SHADER
#version 330

in Fragment_Data {
    vec3 position;
    vec3 world_position;
    vec2 texcoord;
    vec3 normal;
} fragment;

out vec4 frag_color;

// Scene dependant uniforms
struct Light_Setup {
    vec3 position;
    vec3 view_position;
    vec3 color;
    float ambient_intensity;
};

uniform Light_Setup light_setup;
uniform vec3 sky_color;
uniform float fog_density;
uniform float fog_gradient;

// Material depandant uniforms
struct Material {
    vec3 diffuse_color;
    sampler2D diffuse_map;
    sampler2D specular_map;
    float shininess;
};

uniform Material material;

const float specular_intensity = 0.5f;

vec3 apply_fog(in vec3 color) {
    float dist = length(-light_setup.view_position - fragment.world_position);
    float fog_amount = exp(-pow(dist * fog_density, fog_gradient));
    fog_amount = clamp(fog_amount, 0.0f, 1.0f);
    return mix(sky_color, color, fog_amount);
}

void main() {
    // Texture color
    vec3 diffuse_color = vec3(texture2D(material.diffuse_map, fragment.texcoord)) * material.diffuse_color;
    vec3 specular_color = vec3(texture2D(material.specular_map, fragment.texcoord));

    // Ambient light
    vec3 ambient = light_setup.ambient_intensity * diffuse_color;

    // Diffuse light
    vec3 light_direction = normalize(light_setup.position - fragment.world_position);
    float diffuse_amount = max(dot(fragment.normal, light_direction), 0.0f);
    vec3 diffuse = diffuse_amount * diffuse_color;

    // Specular light
    vec3 view_direction = normalize(light_setup.view_position - fragment.world_position);
    vec3 reflect_direction = reflect(-light_direction, fragment.normal);
    float specular_amount = pow(max(dot(view_direction, reflect_direction), 0.0f), material.shininess);
    
    vec3 specular = specular_intensity * specular_amount * specular_color;

    // Calculate the final fragment color
    frag_color = vec4(apply_fog(ambient + diffuse + specular), 1.0f);
}
