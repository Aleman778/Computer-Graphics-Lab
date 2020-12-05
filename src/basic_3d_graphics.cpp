
/***************************************************************************
 * Lab assignment 3 - Alexander Mennborg
 * Basic 3D Graphics using OpenGL
 ***************************************************************************/

struct Basic_3D_Graphics_Scene {
    Mesh cube_mesh;
    Basic_Material basic_material;

    Graphics_Node nodes[100];

    Camera3D camera;

    float light_attenuation;
    float light_intensity;

    bool show_gui;
    
    bool is_initialized;
};

static bool
initialize_scene(Basic_3D_Graphics_Scene* scene) {
    scene->basic_material = compile_basic_material_shader();
    scene->cube_mesh = create_cube_mesh(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));

    initialize_camera_3d(&scene->camera);

    // Setup random number generator for generating cube positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(-5.0f, 5.0f);

    // Create all the graphics nodes
    for (int i = 0; i < array_count(scene->nodes); i++) {
        Graphics_Node* node = &scene->nodes[i];
        node->mesh = &scene->cube_mesh;
        node->material = &scene->basic_material;

        // Setup transform
        glm::vec3 pos(dist(rng)*2.0f, dist(rng)*2.0f, dist(rng) + 5.0f);
        initialize_transform(&node->transform, pos);
    }

    // Set fixed position for the first movable cube
    scene->nodes[0].transform.local_position = glm::vec3(0.0f, 0.0f, 1.0f);

    scene->light_intensity = 0.4f;
    scene->light_attenuation = 20.0f;

    scene->is_initialized = true;
    return true;
}

void
update_and_render_scene(Basic_3D_Graphics_Scene* scene, Window* window) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    // Control the movable cube
    glm::vec3* pos = &scene->nodes[0].transform.local_position;
    if (window->input.a_key.ended_down) pos->x -= 0.02f;
    if (window->input.d_key.ended_down) pos->x += 0.02f;
    if (window->input.w_key.ended_down) pos->y -= 0.02f;
    if (window->input.s_key.ended_down) pos->y += 0.02f;
    if (window->input.e_key.ended_down) pos->z += 0.02f;
    if (window->input.c_key.ended_down) pos->z -= 0.02f;
    
    // TODO(alexander): maybe not do this always
    scene->nodes[0].transform.is_dirty = true; // make sure to update matrix

    // update camera
    update_camera_3d(&scene->camera, (f32) window->width/(f32) window->height);

    // Begin rendering our basic 3D scene
    begin_3d_scene(primary_bg_color, glm::vec4(0, 0, window->width, window->height));
    
    // Enable the basic material shader
    glUseProgram(scene->basic_material.shader);
    glUniform1f(scene->basic_material.u_light_attenuation, scene->light_attenuation);
    glUniform1f(scene->basic_material.u_light_intensity, scene->light_intensity);

    // Bind cube mesh TODO(alexander): this should maybe be moved into the loop, robustness vs. performance!!!
    glBindVertexArray(scene->cube_mesh.vao);

    // Render nodes
    glUniform4fv(scene->basic_material.u_color, 1, glm::value_ptr(green_color));
    for (int i = 0; i < array_count(scene->nodes); i++) {
        Graphics_Node* node = &scene->nodes[i];
        update_transform(&node->transform);
        glm::mat4 transform = scene->camera.combined_matrix * node->transform.matrix;
        glUniformMatrix4fv(scene->basic_material.u_transform, 1, GL_FALSE, glm::value_ptr(transform));
        glDrawElements(GL_TRIANGLES, scene->cube_mesh.count, GL_UNSIGNED_INT, 0);

        if (i == 0) {
            glUniform4fv(scene->basic_material.u_color, 1, glm::value_ptr(magenta_color));
        }
    }

    // Ending our basic 3D scene
    end_3d_scene();

    // ImGui
    ImGui::Begin("Lab 3 - Basic 3D Graphics", &scene->show_gui, ImVec2(280, 350), ImGuiWindowFlags_NoSavedSettings);
    ImGui::SliderFloat("Light intensity", &scene->light_intensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Light attenuation", &scene->light_attenuation, 0.0f, 30.0f);
    ImGui::End();
}
