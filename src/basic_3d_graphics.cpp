
/***************************************************************************
 * Lab assignment 3 - Alexander Mennborg
 * Basic 3D Graphics using OpenGL
 ***************************************************************************/

struct Basic_3D_Graphics_Scene {
    Mesh box;
    GLuint basic_shader;

    bool is_initialized;
};

bool initialize_scene(Basic_3D_Graphics_Scene* scene) {
    scene->basic_shader = load_glsl_shader_from_file("../src/basic_shader.glsl");

    scene->box = create_cube_mesh(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));

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

    
}
