
/***************************************************************************
 * Lab assignment 3 - Alexander Mennborg
 * Basic 3D Graphics using OpenGL
 ***************************************************************************/

struct Basic_3D_Graphics_Scene {
    Mesh box;
    GLuint basic_shader;
    GLint transform_uniform;
    GLint color_uniform;

    glm::mat4 camera_projection;

    bool is_initialized;
};

bool initialize_scene(Basic_3D_Graphics_Scene* scene) {
    static const char* basic_vert_shader_source = R"vs(
#version 330

layout(location=0) in vec3 pos;
uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(pos, 1.0f);
}
)vs";
    
    static const char* basic_frag_shader_source = R"fs(
#version 330

out vec4 frag_color;
uniform vec4 color;

void main() {
    frag_color = color;
}
)fs";

    scene->basic_shader = load_glsl_shader_from_sources(basic_vert_shader_source, basic_frag_shader_source);
    scene->transform_uniform = glGetUniformLocation(scene->basic_shader, "transform");
    if (scene->transform_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `transform`");
        return false;
    }
    scene->color_uniform = glGetUniformLocation(scene->basic_shader, "color");
    if (scene->transform_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `transform`");
        return false;
    }
    
    scene->box = create_cube_mesh(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));

    scene->is_initialized = true;
    return true;
}

void update_and_render_scene(Basic_3D_Graphics_Scene* scene, Window* window) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    // Set viewport
    glViewport(0, 0, window->width, window->height);

    // Render and clear background
    glClearColor(primary_bg_color.x, primary_bg_color.y, primary_bg_color.z, primary_bg_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    static glm::vec3 pos(0.0f);
    static float rot = 0.0f;
    rot += 0.01f;

    if (window->input.a_key.ended_down) pos.x -= 0.02f;
    if (window->input.d_key.ended_down) pos.x += 0.02f;
    if (window->input.w_key.ended_down) pos.y += 0.02f;
    if (window->input.s_key.ended_down) pos.y -= 0.02f;
    if (window->input.e_key.ended_down) pos.z -= 0.02f;
    if (window->input.c_key.ended_down) pos.z += 0.02f;
    
    // Render `box`
    glm::mat4 projection = glm::perspective(90.0f, (f32) window->width/(f32) window->height, 0.1f, 10000.0f);
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rot, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(pos));
    glm::mat4 transform = projection * translation * rotation;
    glUseProgram(scene->basic_shader);
    glUniformMatrix4fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform4f(scene->color_uniform, primary_fg_color.x, primary_fg_color.y, primary_fg_color.z, primary_fg_color.w);
    glBindVertexArray(scene->box.vao);
    glDrawElements(GL_TRIANGLES, scene->box.count, GL_UNSIGNED_INT, 0);
}
