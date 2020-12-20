
/***************************************************************************
 * Lab assignment 4 - Alexander Mennborg
 * Simple 3D world rendered using OpenGL
 ***************************************************************************/

struct Simple_World_Scene {
    Mesh cuboid_mesh;
    Mesh ground_mesh;
    
    Phong_Shader phong_shader;

    Texture texture_default;
    Texture texture_forrest_ground;

    Graphics_Node nodes[100];
    
    Light_Setup light_setup;

    Fps_Camera camera;

    bool show_gui;
    
    bool is_initialized;
};

static bool
initialize_scene(Simple_World_Scene* scene) {
    scene->phong_shader = compile_phong_shader();

    {
        Mesh_Builder mb = {};
        push_cuboid_mesh(&mb, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        scene->cuboid_mesh = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_quad(&mb,
                  glm::vec3(-100.0f, -1.0f, -100.0f), glm::vec2(0.0f,     0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3( 100.0f, -1.0f, -100.0f), glm::vec2(200.0f,   0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3( 100.0f, -1.0f,  100.0f), glm::vec2(200.0f, 200.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(-100.0f, -1.0f,  100.0f), glm::vec2(0.0f,   200.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        scene->ground_mesh = create_mesh_from_builder(&mb);
    }

    // Load textures
    scene->texture_default = generate_white_2d_texture();
    scene->texture_forrest_ground = load_2d_texture_from_file("forrest_ground.png");

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(-5.0f, 5.0f);
    std::uniform_real_distribution<f32> comp(0.0f, 1.0f);

    // Create all the graphics nodes
    for (int i = 0; i < array_count(scene->nodes); i++) {
        Graphics_Node* node = &scene->nodes[i];
        node->material = {};
        node->material.type = Material_Type_Phong;
        node->material.Phong.shader = &scene->phong_shader;
        
        if (i == 0) {
            node->mesh = &scene->ground_mesh;
            node->material.Phong.main_texture = &scene->texture_forrest_ground;
            node->material.Phong.object_color = glm::vec4(1.0f);
        } else {
            node->mesh = &scene->cuboid_mesh;
            node->material.Phong.main_texture = &scene->texture_default;
            node->material.Phong.object_color = primary_fg_color;
        }
 
        node->material.shader = &scene->phong_shader.base;

        // Setup transform
        glm::vec3 pos(dist(rng)*3.0f, dist(rng)*2.0f, dist(rng) - 4.5f);
        float s = (dist(rng) + 5.1f)/5.0f;
        initialize_transform(&node->transform, pos);
    }

    // Setup lgihting 
    scene->light_setup.color = glm::vec3(1.0f, 1.0f, 1.0f);
    scene->light_setup.ambient_intensity = 0.4f;

    // Set fixed position for the first movable cuboid
    scene->nodes[0].transform.local_position = glm::vec3(0.0f, 0.0f, -1.0f);

    // Setup 3D camera
    initialize_fps_camera(&scene->camera);

    scene->is_initialized = true;
    return true;
}

void
update_and_render_scene(Simple_World_Scene* scene, Window* window) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    /***********************************************************************
     * Update
     ***********************************************************************/

    // update camera
    
    update_fps_camera(&scene->camera, &window->input, window->width, window->height);

    /***********************************************************************
     * Rendering
     ***********************************************************************/

    // Begin rendering our basic 3D scene
    begin_scene(primary_bg_color, glm::vec4(0, 0, window->width, window->height), true);
    
    // Apply the basic shader
    apply_shader(&scene->phong_shader.base, &scene->light_setup);

    // Render nodes
    for (int i = 0; i < array_count(scene->nodes); i++) {
        Graphics_Node* node = &scene->nodes[i];
        draw_graphics_node(node, &scene->camera.base);
    }

    // Ending our basic 3D scene
    end_scene();

    // ImGui
    ImGui::Begin("Lab 4 - Simple World", &scene->show_gui, ImVec2(280, 150), ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();
}
