
/***************************************************************************
 * Lab assignment 4 - Alexander Mennborg
 * Simple 3D world rendered using OpenGL
 ***************************************************************************/

struct Player {
    Camera_3D camera;
    f32 rotation_x;
    f32 rotation_y;
    f32 sensitivity;
};

struct Simple_World_Scene {
    Mesh mesh_cube;
    Mesh mesh_terrain;
    Mesh mesh_sphere;
    Mesh mesh_sky;
    Mesh mesh_cylinder;
    Mesh mesh_cone;

    Phong_Shader phong_shader;
    Sky_Shader sky_shader;

    Texture texture_default;
    Texture texture_snow_01_diffuse;
    Texture texture_snow_01_specular;
    Texture texture_snow_02_diffuse;
    Texture texture_snow_02_specular;
    Texture texture_snow_03_diffuse;
    Texture texture_sky;

    std::vector<Graphics_Node*> nodes;

    Light_Setup light_setup;

    glm::vec3 sky_color;

    Player player;

    Height_Map terrain;

    f32 fog_density;
    f32 fog_gradient;

    bool enable_wireframe;
    bool show_gui;

    bool is_initialized;
};

static void
update_player(Player* player, Input* input, Height_Map* terrain, int width, int height) {
    Transform* transform = &player->camera.transform;

    // Looking around, whenever mouse is locked, left click in window (lock), escape (unlock).
    if (input->mouse_locked) {
        f32 delta_x = input->mouse_x - (f32) (width / 2);
        f32 delta_y = input->mouse_y - (f32) (height / 2);

        bool is_dirty = false;
        if (delta_x > 0.1f || delta_x < -0.1f) {
            player->rotation_x += delta_x * player->sensitivity;
            is_dirty = true;
        }

        if (delta_y > 0.1f || delta_y < -0.1f) {
            player->rotation_y += delta_y * player->sensitivity;
            if (player->rotation_y < -1.5f) {
                player->rotation_y = -1.5f;
            }

            if (player->rotation_y > 1.2f) {
                player->rotation_y = 1.2f;
            }
            is_dirty = true;
        }

        if (is_dirty) {
            glm::quat rot_x(glm::vec3(0.0f, player->rotation_x, 0.0f));
            glm::quat rot_y(glm::vec3(player->rotation_y, 0.0f, 0.0f));
            transform->local_rotation = rot_y * rot_x;
            transform->is_dirty = true;
        }
    }

    // Walking
    glm::vec3* pos = &transform->local_position;

    float speed = 0.04f;
    if (input->shift_key.ended_down) {
        speed = 0.08f;
    }

    glm::vec3 forward(cos(player->rotation_x + half_pi)*speed, 0.0f, sin(player->rotation_x + half_pi)*speed);
    glm::vec3 right(cos(player->rotation_x)*speed, 0.0f, sin(player->rotation_x)*speed);
    if (input->w_key.ended_down) *pos += forward;
    if (input->a_key.ended_down) *pos += right;
    if (input->s_key.ended_down) *pos -= forward;
    if (input->d_key.ended_down) *pos -= right;

    // Enable/ disable mouse locking
    if (was_pressed(&input->left_mb)) input->mouse_locked = true;
    if (was_pressed(&input->escape_key)) input->mouse_locked = false;

    // Invisible wall collision
    if (pos->x >   0.0f) pos->x =   0.0f;
    if (pos->x < -99.0f) pos->x = -99.0f;
    if (pos->z >   0.0f) pos->z =   0.0f;
    if (pos->z < -99.0f) pos->z = -99.0f;

    // Gravity
    pos->y += 0.098f; // TODO(alexander): add to velocity

    // Terrain collision
    f32 terrain_height = -sample_point_at(terrain, -pos->x, -pos->z) - 1.0f;
    if (pos->y > terrain_height) {
        pos->y = terrain_height;
    }

    // Update the camera
    transform->is_dirty = true;
    f32 aspect_ratio = 0.0f;
    if (width != 0 && height != 0) {
        aspect_ratio = ((f32) width)/((f32) height);
    }
    update_camera_3d(&player->camera, aspect_ratio);
}

static bool
initialize_scene(Simple_World_Scene* scene) {
    scene->phong_shader = compile_phong_shader();
    scene->sky_shader = compile_sky_shader();

    {
        Mesh_Builder mb = {};
        push_cuboid_mesh(&mb, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        scene->mesh_cube = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        scene->terrain = generate_terrain_mesh(&mb, 100.0f, 100.f, 200, 200, 8, 0.6f, 2.0f, -1.0f);
        for (int i = 0; i < mb.vertices.size(); i++) {
            mb.vertices[i].texcoord *= 0.3f;
        }
        scene->mesh_terrain = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_sphere(&mb, glm::vec3(0.0f), 1.0f);
        scene->mesh_sphere = create_mesh_from_builder(&mb);
        scene->mesh_sky = scene->mesh_sphere;
        scene->mesh_sky.disable_culling = true; // since we are always inside the sky dome
    }

    {
        Mesh_Builder mb = {};
        push_cylinder_triangle_strips(&mb, glm::vec3(0.0f), 0.5f, 2.0f);
        scene->mesh_cylinder = create_mesh_from_builder(&mb);
        scene->mesh_cylinder.mode = GL_TRIANGLE_STRIP;
    }

    {
        Mesh_Builder mb = {};
        push_cone_triangle_strips(&mb, glm::vec3(0.0f), 0.5f, 2.0f);
        scene->mesh_cone = create_mesh_from_builder(&mb);
        scene->mesh_cone.mode = GL_TRIANGLE_STRIP;
    }

    // Load textures
    scene->texture_default          = generate_white_2d_texture();
    scene->texture_snow_01_diffuse  = load_texture_2d_from_file("snow_01_diffuse.png");
    scene->texture_snow_01_specular = load_texture_2d_from_file("snow_01_specular.png");
    scene->texture_snow_02_diffuse  = load_texture_2d_from_file("snow_02_diffuse.png");
    scene->texture_snow_02_specular = load_texture_2d_from_file("snow_02_specular.png");
    scene->texture_snow_03_diffuse  = load_texture_2d_from_file("snow_03_diffuse.png");
    scene->texture_sky              = load_texture_2d_from_file("satara_night_no_lamps_2k.hdr");
    // scene->texture_sky        = load_texture_2d_from_file("winter_lake_01_1k.hdr");

    // Scene properties
    scene->sky_color = glm::vec3(0.01f, 0.01f, 0.01f);
    scene->fog_density = 0.05f;
    scene->fog_gradient = 2.0f;
    scene->enable_wireframe = false;
    scene->light_setup.position = glm::vec3(50.0f, 1.0f, 50.0f);
    scene->light_setup.color = glm::vec3(1.0f, 1.0f, 1.0f);
    scene->light_setup.ambient_intensity = 0.4f;

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(0.0f, 100.0f);
    std::uniform_real_distribution<f32> comp(0.0f, 1.0f);

    // Sky
    {
        Graphics_Node* node = new Graphics_Node();
        scene->nodes.push_back(node);

        initialize_transform(&node->transform);
        node->mesh = &scene->mesh_sky;
        node->material.type = Material_Type_Sky;
        node->material.Sky.color = scene->sky_color;
        node->material.Sky.map = &scene->texture_sky;
        node->material.Sky.shader = &scene->sky_shader;
        node->material.shader = &scene->sky_shader.base;
        node->transform.local_position = glm::vec3(0.0f);
        node->transform.local_scale = glm::vec3(100.0f);
    }

    {
        Graphics_Node* node = new Graphics_Node();
        scene->nodes.push_back(node);

        node->material = {};
        initialize_transform(&node->transform);
        node->mesh = &scene->mesh_terrain;

        node->material.type = Material_Type_Phong;
        node->material.Phong.diffuse_color = glm::vec3(1.0f);
        node->material.Phong.diffuse_map = &scene->texture_snow_01_diffuse;
        node->material.Phong.specular_map = &scene->texture_snow_01_specular;
        node->material.Phong.shininess = 2.0f;
        node->material.Phong.shader = &scene->phong_shader;
        node->material.shader = &scene->phong_shader.base;
    }

    for (int i = 0; i < 98; i++) {
        Graphics_Node* node = new Graphics_Node();
        scene->nodes.push_back(node);

        glm::vec3 pos(dist(rng), 0.0f, dist(rng));
        pos.y = sample_point_at(&scene->terrain, pos.x, pos.z) + 0.5f;
        initialize_transform(&node->transform, pos);
        node->mesh = &scene->mesh_sphere;
        node->material.type = Material_Type_Phong;
        node->material.Phong.diffuse_color = glm::vec3(1.0f);
        node->material.Phong.diffuse_map = &scene->texture_snow_02_diffuse;
        node->material.Phong.specular_map = &scene->texture_snow_02_diffuse;
        node->material.Phong.shininess = 2.0f;
        node->material.Phong.shader = &scene->phong_shader;
        node->material.shader = &scene->phong_shader.base;
    }

    // Setup player and its camera
    initialize_camera_3d(&scene->player.camera);
    scene->player.camera.transform.local_position.x = -50.0f;
    scene->player.camera.transform.local_position.z = -50.0f;
    scene->player.sensitivity = 0.01f;

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

    update_player(&scene->player, &window->input, &scene->terrain, window->width, window->height);
    scene->light_setup.view_position = scene->player.camera.transform.local_position;

    /***********************************************************************
     * Rendering
     ***********************************************************************/

    // Begin rendering our basic 3D scene
    begin_scene(glm::vec4(scene->sky_color, 1.0f), glm::vec4(0, 0, window->width, window->height), true);

    // Move sky to camera position
    {
        Graphics_Node* node = scene->nodes[0];
        node->transform.local_position = -scene->player.camera.transform.local_position;
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
        draw_graphics_node(node, &scene->player.camera);
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
