
/***************************************************************************
 * Vertex Shader
 ***************************************************************************/

#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 a_pos;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in vec3 a_normal;

out vec3 frag_pos;
out vec2 texcoord;
out vec3 normal;

uniform mat4 model_transform;
uniform mat4 mvp_transform;

void main() {
    frag_pos = vec3(model_transform * vec4(a_pos, 1.0f));
    texcoord = a_texcoord;
    normal = a_normal;

    gl_Position = mvp_transform * vec4(a_pos, 1.0f);
}

/***************************************************************************
 * Fragment Shader
 ***************************************************************************/

#shader GL_FRAGMENT_SHADER
#version 330

in vec3 frag_pos;
in vec2 texcoord;
in vec3 normal;

out vec4 frag_color;

struct Light_Setup {
    vec3 pos;
    vec3 view_pos;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Material {
    vec3 color;
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

uniform Material material;

uniform Light_Setup light;

uniform vec3 fog_color;
uniform float fog_density;
uniform float fog_gradient;

void main() {
    vec3 diffuse_texel = texture2D(material.diffuse, texcoord).rgb;
    vec3 specular_texel = texture2D(material.specular, texcoord).rgb;

    // Ambient light
    vec3 ambient = diffuse_texel * material.color * light.ambient;

    // Calulate diffuse color
    vec3 diffuse_color = diffuse_texel * material.color * light.diffuse;
    vec3 light_dir = normalize(light.pos - frag_pos);
    float diffuse_amount = max(dot(normal, light_dir), 0.0f);
    vec3 diffuse = diffuse_amount * diffuse_color;

    // Calculate specular light
    vec3 specular_color = specular_texel * light.specular;
    vec3 view_dir = normalize(light.view_pos - frag_pos);
    vec3 reflect_dir = normalize(reflect(-light_dir, normal));
    float specular_amount = pow(max(dot(view_dir, reflect_dir), 0.0f), material.shininess);
    vec3 specular = specular_amount * specular_color;

    // Final phong shading color
    vec3 phong_color = ambient + diffuse + specular;

    // Calculate the amount of fog
    float dist = length(light.view_pos - frag_pos);
    float fog_amount = exp(-pow(dist * fog_density, fog_gradient));
    fog_amount = clamp(fog_amount, 0.0f, 1.0f);

    // Calculate the final fragment color
    frag_color = vec4(mix(fog_color, phong_color, fog_amount), 1.0f); 
}
