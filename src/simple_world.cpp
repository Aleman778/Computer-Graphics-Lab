
/***************************************************************************
 * Lab assignment 4 - Alexander Mennborg
 * Simple 3D world rendered using OpenGL
 ***************************************************************************/

struct Simple_World_Scene {
    Mesh cube_mesh;
    Mesh ground_mesh;
    Mesh sphere_mesh;
    Mesh sky_mesh;
    Mesh cylinder_mesh;
    Mesh cone_mesh;

    Phong_Shader phong_shader;
    Sky_Shader sky_shader;

    Texture texture_default;
    Texture texture_snow_01_diffuse;
    Texture texture_snow_01_specular;
    Texture texture_snow_02_diffuse;
    Texture texture_snow_02_specular;
    Texture texture_night_sky;
    Texture texture_test;

    std::vector<Graphics_Node*> nodes;

    Light_Setup light_setup;

    glm::vec3 sky_color;

    Fps_Camera camera;

    f32 fog_density;
    f32 fog_gradient;

    bool enable_wireframe;
    bool show_gui;

    bool is_initialized;
};

static bool
initialize_scene(Simple_World_Scene* scene) {
    scene->phong_shader = compile_phong_shader();
    scene->sky_shader = compile_sky_shader();

    {
        Mesh_Builder mb = {};
        push_cuboid_mesh(&mb, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        scene->cube_mesh = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_flat_plane(&mb, glm::vec3(-50.0, -1.0, -50.0), 100, 100);
        scene->ground_mesh = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_sphere(&mb, glm::vec3(0.0f), 1.0f);
        scene->sphere_mesh = create_mesh_from_builder(&mb);
        scene->sky_mesh = scene->sphere_mesh;
        scene->sky_mesh.disable_culling = true; // since we are always inside the sky dome
    }

    {
        Mesh_Builder mb = {};
        push_cylinder_triangle_strips(&mb, glm::vec3(0.0f), 0.5f, 2.0f);
        scene->cylinder_mesh = create_mesh_from_builder(&mb);
        scene->cylinder_mesh.mode = GL_TRIANGLE_STRIP;
    }

    {
        Mesh_Builder mb = {};
        push_cone_triangle_strips(&mb, glm::vec3(0.0f), 0.5f, 2.0f);
        scene->cone_mesh = create_mesh_from_builder(&mb);
        scene->cone_mesh.mode = GL_TRIANGLE_STRIP;
    }
    
    // Load textures
    scene->texture_default          = generate_white_2d_texture();
    scene->texture_snow_01_diffuse  = load_2d_texture_from_file("snow_01_diffuse.png");
    scene->texture_snow_01_specular = load_2d_texture_from_file("snow_01_specular.png");
    scene->texture_snow_02_diffuse  = load_2d_texture_from_file("snow_02_diffuse.png");
    scene->texture_snow_02_specular = load_2d_texture_from_file("snow_02_specular.png");
    scene->texture_test             = load_2d_texture_from_file("test.png");
    scene->texture_night_sky        = load_2d_texture_from_file("satara_night_no_lamps_2k.hdr");
    // scene->texture_night_sky        = load_2d_texture_from_file("winter_lake_01_1k.hdr");

    // Scene properties
    scene->sky_color = glm::vec3(0.01f, 0.01f, 0.01f);
    scene->fog_density = 0.01f;
    scene->fog_gradient = 2.0f;
    scene->enable_wireframe = false;
    scene->light_setup.position = glm::vec3(0.0f, 1.0f, 0.0f);
    scene->light_setup.color = glm::vec3(1.0f, 1.0f, 1.0f);
    scene->light_setup.ambient_intensity = 0.4f;

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(-10.0f, 10.0f);
    std::uniform_real_distribution<f32> comp(0.0f, 1.0f);

    // Sky
    {
        Graphics_Node* node = new Graphics_Node();
        scene->nodes.push_back(node);

        initialize_transform(&node->transform);
        node->mesh = &scene->sky_mesh;
        node->material.type = Material_Type_Sky;
        node->material.Sky.color = scene->sky_color;
        node->material.Sky.map = &scene->texture_night_sky;
        node->material.Sky.shader = &scene->sky_shader;
        node->material.shader = &scene->sky_shader.base;
        node->transform.local_position = glm::vec3(0.0f);
        node->transform.local_scale = glm::vec3(100.0f);
    }
    
    // Ground
    {
        Graphics_Node* node = new Graphics_Node();
        scene->nodes.push_back(node);
        
        node->material = {};
        initialize_transform(&node->transform);
        node->mesh = &scene->ground_mesh;
                                
        node->material.type = Material_Type_Phong;
        node->material.Phong.diffuse_color = glm::vec3(1.0f);
        node->material.Phong.diffuse_map = &scene->texture_snow_01_diffuse;
        node->material.Phong.specular_map = &scene->texture_snow_01_specular;
        node->material.Phong.shininess = 2.0f;
        node->material.Phong.shader = &scene->phong_shader;
        node->material.shader = &scene->phong_shader.base;
    }

    // Snowman
    for (int i = 0; i < 98; i++) {
        Graphics_Node* node = new Graphics_Node();
        scene->nodes.push_back(node);
        
        glm::vec3 pos(dist(rng), dist(rng)*0.5f + 4.0f, dist(rng));
        initialize_transform(&node->transform, pos);
        node->mesh = &scene->sphere_mesh;
        node->material.type = Material_Type_Phong;
        node->material.Phong.diffuse_color = glm::vec3(1.0f);
        node->material.Phong.diffuse_map = &scene->texture_snow_02_diffuse;
        node->material.Phong.specular_map = &scene->texture_snow_02_diffuse;
        node->material.Phong.shininess = 2.0f;
        node->material.Phong.shader = &scene->phong_shader;
        node->material.shader = &scene->phong_shader.base;
    }

    
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
    scene->light_setup.view_position = scene->camera.base.transform.local_position;

    /***********************************************************************
     * Rendering
     ***********************************************************************/

    // Begin rendering our basic 3D scene
    begin_scene(glm::vec4(scene->sky_color, 1.0f), glm::vec4(0, 0, window->width, window->height), true);

    // Move the light
    // {
        // static f32 x = 0.0f;
        // x += 0.01f;
        // Graphics_Node* node = scene->nodes[1];
        // node->transform.local_position = glm::vec3(cos(x), 1.0f, sin(x));
        // node->transform.is_dirty = true;
        // scene->light_setup.position = node->transform.local_position;
    // }

    // Move sky to camera position
    {
        Graphics_Node* node = scene->nodes[0];
        node->transform.local_position = -scene->camera.base.transform.local_position;
        node->material.Sky.color = scene->sky_color;
        node->transform.is_dirty = true;
    }

    

    // Enable wireframe if enabled
    if (scene->enable_wireframe) {
        glLineWidth(2);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Render nodes
    Material_Type prev_material = Material_Type_None;
    for (int i = 0; i < scene->nodes.size(); i++) {
        Graphics_Node* node = scene->nodes[i];
        if (node->material.type != prev_material) {
            apply_shader(node->material.shader,
                         &scene->light_setup,
                         scene->sky_color,
                         scene->fog_density,
                         scene->fog_gradient);
        }
        draw_graphics_node(node, &scene->camera.base);
    }

    // Ending our basic 3D scene
    end_scene();

    // ImGui
    ImGui::Begin("Lab 4 - Simple World", &scene->show_gui, ImVec2(280, 150), ImGuiWindowFlags_NoSavedSettings);
    ImGui::ColorEdit3("Sky Color", &scene->sky_color.x);
    ImGui::Text("Fog:");
    ImGui::SliderFloat("Density", &scene->fog_density, 0.01f, 0.5f);
    ImGui::SliderFloat("Gradient", &scene->fog_gradient, 1.0f, 10.0f);
    ImGui::Checkbox("Wireframe mode", &scene->enable_wireframe);
    ImGui::End();
}
