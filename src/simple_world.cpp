
/***************************************************************************
 * Lab assignment 4 - Alexander Mennborg
 * Simple 3D world rendered using OpenGL
 ***************************************************************************/

struct Simple_World_Scene {
    Mesh cuboid_mesh;
    Mesh ground_mesh;
    Mesh sphere_mesh;
    
    Phong_Shader phong_shader;

    Texture texture_default;
    Texture texture_snow_01_diffuse;
    Texture texture_snow_01_specular;
    Texture texture_snow_02_diffuse;
    Texture texture_snow_02_specular;
    Texture texture_night_sky;
    Texture texture_test;

    Graphics_Node nodes[100];
    
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

    {
        Mesh_Builder mb = {};
        push_cuboid_mesh(&mb, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        scene->cuboid_mesh = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_quad(&mb,
                  glm::vec3(-100.0f, -1.0f,  100.0f), glm::vec2(0.0f,     0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3( 100.0f, -1.0f,  100.0f), glm::vec2(100.0f,   0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3( 100.0f, -1.0f, -100.0f), glm::vec2(100.0f, 100.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(-100.0f, -1.0f, -100.0f), glm::vec2(0.0f,   100.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        scene->ground_mesh = create_mesh_from_builder(&mb);
    }
    
    {
        Mesh_Builder mb = {};
        push_sphere(&mb, glm::vec3(0.0f), 1.0f);
        scene->sphere_mesh = create_mesh_from_builder(&mb);
        scene->sphere_mesh.mode = GL_TRIANGLE_STRIP;
    }

    // Load textures
    scene->texture_default          = generate_white_2d_texture();
    scene->texture_snow_01_diffuse  = load_2d_texture_from_file("snow_01_diffuse.png");
    scene->texture_snow_01_specular = load_2d_texture_from_file("snow_01_specular.png");
    scene->texture_snow_02_diffuse  = load_2d_texture_from_file("snow_02_diffuse.png");
    scene->texture_snow_02_specular = load_2d_texture_from_file("snow_02_specular.png");
    scene->texture_test             = load_2d_texture_from_file("test.png");
    scene->texture_night_sky        = load_2d_texture_from_file("satara_night_no_lamps_1k.hdr");
    // scene->texture_night_sky        = load_2d_texture_from_file("winter_lake_01_1k.hdr");

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(-10.0f, 10.0f);
    std::uniform_real_distribution<f32> comp(0.0f, 1.0f);

    // Create all the graphics nodes
    for (int i = 0; i < array_count(scene->nodes); i++) {
        Graphics_Node* node = &scene->nodes[i];
        node->material = {};
        node->material.type = Material_Type_Phong;
        node->material.Phong.shader = &scene->phong_shader;

        // Setup transform
        if (i == 0) {
            initialize_transform(&node->transform);
            node->mesh = &scene->ground_mesh;
            node->material.Phong.diffuse_color = glm::vec3(1.0f);
            node->material.Phong.diffuse_map = &scene->texture_snow_01_diffuse;
            node->material.Phong.specular_map = &scene->texture_snow_01_diffuse;
            node->material.Phong.shininess = 2.0f;
        } else {
            glm::vec3 pos(dist(rng), dist(rng)*0.5f + 4.0f, dist(rng));
            initialize_transform(&node->transform, pos);
        
            node->mesh = &scene->sphere_mesh;
            if (i == 1) {
                node->material.Phong.diffuse_color = glm::vec3(1.0f);
                node->material.Phong.diffuse_map = &scene->texture_default;
                node->material.Phong.specular_map = &scene->texture_default;
                node->transform.local_scale = glm::vec3(0.2f, 0.2f, 0.2f);
            } else if (i == 2) {
                node->material.Phong.diffuse_color = glm::vec3(1.0f);
                node->material.Phong.diffuse_map = &scene->texture_night_sky;
                node->material.Phong.specular_map = &scene->texture_default;
                node->transform.local_position = glm::vec3(0.0f);
                node->transform.local_scale = glm::vec3(100.0f);
            } else {
                node->material.Phong.diffuse_color = glm::vec3(1.0f);
                node->material.Phong.diffuse_map = &scene->texture_snow_02_diffuse;
                node->material.Phong.specular_map = &scene->texture_snow_02_diffuse;
                node->material.Phong.shininess = 2.0f;
            }
        }
 
        node->material.shader = &scene->phong_shader.base;
    }

    // Setup lgihting
    scene->light_setup.position = glm::vec3(0.0f, 1.0f, 0.0f);
    scene->light_setup.color = glm::vec3(1.0f, 1.0f, 1.0f);
    scene->light_setup.ambient_intensity = 0.4f;

    // Sky color
    scene->sky_color = glm::vec3(0.01f, 0.04f, 0.08f);
    scene->fog_density = 0.01f;
    scene->fog_gradient = 2.0f;
    scene->enable_wireframe = false;

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
    
    // Apply the basic shader
    apply_shader(&scene->phong_shader.base,
                 &scene->light_setup, 
                 scene->sky_color, 
                 scene->fog_density, 
                 scene->fog_gradient);

    // Move the light
    {
        static f32 x = 0.0f;
        x += 0.01f;
        Graphics_Node* node = &scene->nodes[1];
        node->transform.local_position = glm::vec3(cos(x), 1.0f, sin(x));
        node->transform.is_dirty = true;
        scene->light_setup.position = node->transform.local_position;
    }


    // Move environment map to camera position
    {
        Graphics_Node* node = &scene->nodes[2];
        node->transform.local_position = -scene->camera.base.transform.local_position;
        node->transform.local_position.y = 10.0f;
        node->transform.is_dirty = true;
    }
    
    if (scene->enable_wireframe) {
        glLineWidth(3);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Render nodes
    for (int i = 0; i < array_count(scene->nodes); i++) {
        Graphics_Node* node = &scene->nodes[i];
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
