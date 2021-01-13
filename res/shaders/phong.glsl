
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
uniform mat3 normal_transform;
uniform mat4 mvp_transform;

void main() {
    frag_pos = vec3(model_transform * vec4(a_pos, 1.0f));
    texcoord = a_texcoord;
    normal = normal_transform * a_normal;

    gl_Position = mvp_transform * vec4(a_pos, 1.0f);
}

/***************************************************************************
 * Fragment Shader
 ***************************************************************************/

#shader GL_FRAGMENT_SHADER
#version 330

#define MAX_POINT_LIGHTS 2

in vec3 frag_pos;
in vec2 texcoord;
in vec3 normal;

out vec4 frag_color;

struct Directional_Light {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Point_Light {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
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

uniform Directional_Light directional_light;
uniform Point_Light point_lights[MAX_POINT_LIGHTS];
uniform vec3 view_pos;

uniform vec3 fog_color;
uniform float fog_density;
uniform float fog_gradient;


vec3 calc_directional_light(Directional_Light light, vec3 view_dir) {
    vec3 light_dir = normalize(-light.direction);
    float diff = max(dot(normal, light_dir), 0.0f);
    
    vec3 reflect_dir = normalize(reflect(-light_dir, normal));
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), material.shininess);

    vec3 ambient  =        light.ambient  * texture2D(material.diffuse,  texcoord).rgb;
    vec3 diffuse  = diff * light.diffuse  * texture2D(material.diffuse,  texcoord).rgb;
    vec3 specular = spec * light.specular * texture2D(material.specular, texcoord).rgb;
    
    return (ambient + diffuse + specular) * material.color;
}

vec3 calc_point_light(Point_Light light, vec3 view_dir) {
    vec3 light_dir = normalize(light.position - frag_pos);
    float diff = max(dot(normal, light_dir), 0.0f);
    
    vec3 reflect_dir = normalize(reflect(-light_dir, normal));
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), material.shininess);

    float dist = length(light.position - frag_pos);
    float denominator = light.constant;
    denominator += light.linear*dist;
    denominator += light.quadratic*dist;
    // float attenuation = 1.0f/(light.linear*dist + light.quadratic*dist*dist);
    float attenuation = 1.0f/denominator;

    vec3 ambient  =        light.ambient  * texture2D(material.diffuse,  texcoord).rgb;
    vec3 diffuse  = diff * light.diffuse  * texture2D(material.diffuse,  texcoord).rgb;
    vec3 specular = spec * light.specular * texture2D(material.specular, texcoord).rgb;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular) * material.color;
}

void main() {
    vec3 view_dir = normalize(view_pos - frag_pos);
    
    // Calculate lighting
    vec3 phong_color = calc_directional_light(directional_light, view_dir);
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        phong_color += calc_point_light(point_lights[i], view_dir);
    }
    
    // Calculate the amount of fog
    float dist = length(view_pos - frag_pos);
    float fog_amount = exp(-pow(dist * fog_density, fog_gradient));
    fog_amount = clamp(fog_amount, 0.0f, 1.0f);

    // Calculate the final fragment color
    frag_color = vec4(mix(fog_color, phong_color, fog_amount), 1.0f); 
}
