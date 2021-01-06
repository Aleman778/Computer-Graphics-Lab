
/***************************************************************************
 * Lab assignment 4 - Alexander Mennborg
 * Simple 3D world rendered using OpenGL
 ***************************************************************************/

struct Player_Controller {
    Entity player;
    Entity camera;
    glm::vec3 position;
    glm::vec2 rotation;
    f32 sensitivity;
};

struct Simple_World_Scene {
    World world;
    Entity camera;

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
    Texture texture_sky;

    Player_Controller player_controller;

    Height_Map terrain;

    bool enable_wireframe;
    bool show_gui;

    bool is_initialized;
};

static void
player_controller(World* world, Player_Controller* pc, Input* input, Height_Map* terrain, int width, int height) {
    // Looking around, whenever mouse is locked, left click in window to lock, press escape to unlock.
    if (input->mouse_locked) {
        f32 delta_x = input->mouse_x - (f32) (width / 2);
        f32 delta_y = input->mouse_y - (f32) (height / 2);

        bool is_dirty = false;
        if (delta_x > 0.1f || delta_x < -0.1f) {
            pc->rotation.x += delta_x * pc->sensitivity;
            is_dirty = true;
        }

        if (delta_y > 0.1f || delta_y < -0.1f) {
            pc->rotation.y += delta_y * pc->sensitivity;
            if (pc->rotation.y < -1.5f) {
                pc->rotation.y = -1.5f;
            }

            if (pc->rotation.y > 1.2f) {
                pc->rotation.y = 1.2f;
            }
        }
    }
    
    // Walking
    float speed = 0.04f;
    if (input->shift_key.ended_down) {
        speed = 0.08f;
    }

    glm::vec3 forward(cos(pc->rotation.x + half_pi)*speed, 0.0f, sin(pc->rotation.x + half_pi)*speed);
    glm::vec3 right(cos(pc->rotation.x)*speed, 0.0f, sin(pc->rotation.x)*speed);
    if (input->w_key.ended_down) pc->position += forward;
    if (input->a_key.ended_down) pc->position += right;
    if (input->s_key.ended_down) pc->position -= forward;
    if (input->d_key.ended_down) pc->position -= right;

    // Enable/ disable mouse locking
    if (was_pressed(&input->left_mb)) input->mouse_locked = true;
    if (was_pressed(&input->escape_key)) input->mouse_locked = false;

    // Invisible wall collision
    if (pc->position.x >   0.0f) pc->position.x =   0.0f;
    if (pc->position.x < -99.5f) pc->position.x = -99.5f;
    if (pc->position.z >   0.0f) pc->position.z =   0.0f;
    if (pc->position.z < -99.5f) pc->position.z = -99.5f;

    // Gravity
    pc->position.y += 0.098f; // TODO(alexander): add to velocity

    // Terrain collision
    f32 terrain_height = -sample_point_at(terrain, -pc->position.x, -pc->position.z) - 1.0f;
    if (pc->position.y > terrain_height) {
        pc->position.y = terrain_height;
    }

    glm::quat rot_x(glm::vec3(0.0f, pc->rotation.x, 0.0f));
    glm::quat rot_y(glm::vec3(pc->rotation.y, 0.0f, 0.0f));
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), pc->position);
    transform = glm::toMat4(rot_y * rot_x)*transform;
 
    set_local_transform(world, pc->player, transform);
    set_local_transform(world, pc->camera, transform);

    Camera_Data* camera = lookup_camera(&world->cameras, pc->camera);
    if (camera) camera->view_position = pc->position;
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
        scene->mesh_sky.is_two_sided = true; // since we are always inside the sky dome
    }

    {
        Mesh_Builder mb = {};
        push_sphere(&mb, glm::vec3(0.0f), 100.0f);
        scene->mesh_sky = create_mesh_from_builder(&mb);
        scene->mesh_sky.is_two_sided = true; // since we are always inside the sky dome
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
    scene->texture_sky              = load_texture_2d_from_file("satara_night_no_lamps_2k.hdr");
    // scene->texture_sky        = load_texture_2d_from_file("winter_lake_01_1k.hdr");

    // Scene and world properties
    scene->world.clear_color = glm::vec4(0.01f, 0.01f, 0.01f, 1.0f);
    scene->world.depth_testing = true;
    scene->world.fog_density = 0.05f;
    scene->world.fog_gradient = 2.0f;
    scene->world.light_setup.position = glm::vec3(50.0f, 1.0f, 50.0f);
    scene->world.light_setup.color = glm::vec3(1.0f, 1.0f, 1.0f);
    scene->world.light_setup.ambient_intensity = 0.4f;
    scene->enable_wireframe = false;

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(0.0f, 100.0f);
    std::uniform_real_distribution<f32> comp(0.0f, 1.0f);

    // Create world
    scene->camera = spawn_entity(&scene->world);
    scene->world.main_camera = scene->camera;
    glm::mat4 camera_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    set_local_transform(&scene->world, scene->camera, camera_transform);
    set_perspective(&scene->world, scene->camera);

    Entity player = spawn_entity(&scene->world);
    set_local_transform(&scene->world, player, camera_transform);
    scene->player_controller.player = player;
    scene->player_controller.camera = scene->camera;
    scene->player_controller.position.x = -50.0f;
    scene->player_controller.position.y =  50.0f;
    scene->player_controller.position.z = -50.0f;
    scene->player_controller.sensitivity = 0.01f;

    Material sky_material = {};
    sky_material.type = Material_Type_Sky;
    sky_material.Sky.map = &scene->texture_sky;
    sky_material.Sky.shader = &scene->sky_shader;

    Entity sky = spawn_entity(&scene->world);
    set_mesh(&scene->world, sky, scene->mesh_sky, sky_material);

    Material snow_ground_material = {};
    snow_ground_material.type = Material_Type_Phong;
    snow_ground_material.Phong.diffuse_color = glm::vec3(1.0f);
    snow_ground_material.Phong.diffuse_map = &scene->texture_snow_01_diffuse;
    snow_ground_material.Phong.specular_map = &scene->texture_snow_01_specular;
    snow_ground_material.Phong.shininess = 2.0f;
    snow_ground_material.Phong.shader = &scene->phong_shader;

    Entity terrain = spawn_entity(&scene->world);
    set_mesh(&scene->world, terrain, scene->mesh_terrain, snow_ground_material);

    Material snow_material = snow_ground_material;
    snow_material.Phong.diffuse_map = &scene->texture_snow_02_diffuse;
    snow_material.Phong.specular_map = &scene->texture_snow_02_specular;

    for (int i = 0; i < 80; i++) {
        Entity snowball = spawn_entity(&scene->world);
        glm::vec3 pos(dist(rng), 0.0f, dist(rng));
        pos.y = sample_point_at(&scene->terrain, pos.x, pos.z) + 0.5f;
        set_mesh(&scene->world, snowball, scene->mesh_sphere, snow_material);
        set_local_transform(&scene->world, snowball, glm::translate(glm::mat4(1.0f), pos));
    }

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

    player_controller(&scene->world,
                      &scene->player_controller,
                      &window->input,
                      &scene->terrain,
                      window->width,
                      window->height);
    
    adjust_camera_to_fill_window(&scene->world, scene->camera, window->width, window->height);

    // Use wireframe if enabled
    if (scene->enable_wireframe) {
        glLineWidth(2);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    render_world(&scene->world);
    
    ImGui::Begin("Lab 4 - Simple World", &scene->show_gui, ImVec2(280, 150), ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Light Setup:");
    ImGui::ColorEdit3("Light color", &scene->world.light_setup.color.x);

    ImGui::Text("Sky:");
    ImGui::ColorEdit3("Sky color", &scene->world.clear_color.x);
    
    ImGui::Text("Fog:");
    ImGui::SliderFloat("Density", &scene->world.fog_density, 0.01f, 0.5f);
    ImGui::SliderFloat("Gradient", &scene->world.fog_gradient, 1.0f, 10.0f);
    ImGui::Checkbox("Wireframe mode", &scene->enable_wireframe);
    ImGui::End();
}
