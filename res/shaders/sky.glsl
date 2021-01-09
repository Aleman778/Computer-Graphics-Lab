
/***************************************************************************
 * Vertex Shader
 ***************************************************************************/

#shader GL_VERTEX_SHADER
#version 330

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec3 normal;

out Fragment_Data {
    vec2 texcoord;
} fragment;

uniform mat4 vp_transform;

void main() {
    fragment.texcoord = texcoord;
    gl_Position = vp_transform * vec4(position, 1.0f);
}

/***************************************************************************
 * Fragment Shader
 ***************************************************************************/

#shader GL_FRAGMENT_SHADER
#version 330

in Fragment_Data {
    vec2 texcoord;
} fragment;

out vec4 frag_color;

// Material depandant uniforms
struct Material {
    sampler2D map;
    vec3 fog_color;
};

uniform Material material;

void main() {
    vec4 fog_color = vec4(material.fog_color, 1.0f);
    vec4 texel_color = texture2D(material.map, fragment.texcoord);
    float factor = 1.0f - exp(-pow(fragment.texcoord.y + 0.50f, 40.0f));
    frag_color = mix(texel_color, fog_color, factor);
}
