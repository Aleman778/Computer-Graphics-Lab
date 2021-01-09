
/***************************************************************************
 * Lab assignment 4 - Alexander Mennborg
 * Simple 3D world rendered using OpenGL
 ***************************************************************************/

struct Simple_World_Scene {
    World world;
    std::vector<System> main_systems;
    std::vector<System> rendering_pipeline;
    Entity_Handle player;
    Entity_Handle player_camera;
    f32 sensitivity;

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

    Height_Map terrain;

    bool enable_wireframe;
    bool show_gui;

    bool is_initialized;
};

void
control_player(World* world,
               Entity_Handle player,
               Entity_Handle camera,
               Input* input,
               Height_Map* terrain,
               f32 sensitivity) {

    auto pos = get_component(world, player, Position);
    auto rot = get_component(world, player, Euler_Rotation);

    // Looking around, whenever mouse is locked, left click in window to lock, press escape to unlock.
    if (input->mouse_locked) {
        f32 delta_x = input->mouse_delta_x;
        f32 delta_y = input->mouse_delta_y;

        bool is_dirty = false;
        if (delta_x > 0.1f || delta_x < -0.1f) {
            rot->v.x += delta_x * sensitivity;
            is_dirty = true;
        }

        if (delta_y > 0.1f || delta_y < -0.1f) {
            rot->v.y += delta_y * sensitivity;
            if (rot->v.y < -1.5f) {
                rot->v.y = -1.5f;
            }

            if (rot->v.y > 1.2f) {
                rot->v.y = 1.2f;
            }
        }
    }

    // Walking
    float speed = 0.04f;
    if (input->shift_key.ended_down) {
        speed = 0.08f;
    }

    glm::vec3 forward(cos(rot->v.x + pi + half_pi)*speed, 0.0f, sin(rot->v.x + pi + half_pi)*speed);
    glm::vec3 right(cos(rot->v.x + pi)*speed, 0.0f, sin(rot->v.x + pi)*speed);
    if (input->w_key.ended_down) pos->v += forward;
    if (input->a_key.ended_down) pos->v += right;
    if (input->s_key.ended_down) pos->v -= forward;
    if (input->d_key.ended_down) pos->v -= right;

    // Enable/ disable mouse locking
    if (was_pressed(&input->left_mb)) input->mouse_locked = true;
    if (was_pressed(&input->escape_key)) input->mouse_locked = false;

    // Invisible wall collision
    if (pos->v.x <  0.0f) pos->v.x =   0.0f;
    if (pos->v.x > 99.5f) pos->v.x = 99.5f;
    if (pos->v.z <  0.0f) pos->v.z =   0.0f;
    if (pos->v.z > 99.5f) pos->v.z = 99.5f;

    // Gravity
    pos->v.y -= 0.098f; // TODO(alexander): add to velocity

    // Terrain collision
    f32 terrain_height = sample_point_at(terrain, pos->v.x, pos->v.z) + 1.8f;
    if (pos->v.y < terrain_height) {
        pos->v.y = terrain_height;
    }

    // Update the camera to players position
    auto camera_pos = get_component(world, camera, Position);
    auto camera_rot = get_component(world, camera, Euler_Rotation);

    if (camera_pos) camera_pos->v = pos->v;
    if (camera_rot) camera_rot->v = rot->v;
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
    scene->world.fog_density = 0.05f;
    scene->world.fog_gradient = 2.0f;
    scene->world.light.pos = glm::vec3(50.0f, 1.0f, 50.0f);
    scene->world.light.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    scene->world.light.diffuse = glm::vec3(0.6f, 0.6f, 0.6f);
    scene->world.light.specular = glm::vec3(0.5f, 0.5f, 0.5f);
    scene->enable_wireframe = false;

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(0.0f, 100.0f);
    std::uniform_real_distribution<f32> comp(0.0f, 1.0f);

    // Setup some reusable materials
    Material snow_ground_material = {};
    snow_ground_material.type = Material_Type_Phong;
    snow_ground_material.Phong.color = glm::vec3(1.0f);
    snow_ground_material.Phong.diffuse = &scene->texture_snow_01_diffuse;
    snow_ground_material.Phong.specular = &scene->texture_snow_01_specular;
    snow_ground_material.Phong.shininess = 32.0f;
    snow_ground_material.Phong.shader = &scene->phong_shader;

    Material snow_material = snow_ground_material;
    snow_material.Phong.diffuse = &scene->texture_snow_02_diffuse;
    snow_material.Phong.specular = &scene->texture_snow_02_specular;

    // Setup the world
    World* world = &scene->world;

    // Player
    Entity_Handle player = spawn_entity(world);
    scene->player = player;
    add_component(world, player, Local_To_World);
    auto pos = add_component(world, player, Position);
    pos->v = glm::vec3(50.0f, -50.0f, 50.0f);
    add_component(world, player, Rotation);
    add_component(world, player, Euler_Rotation);

    // Player Camera
    Entity_Handle player_camera = spawn_entity(world);
    scene->player_camera = player_camera;
    auto camera = add_component(world, player_camera, Camera);
    camera->fov = half_pi;
    camera->near = 0.01f;
    camera->far = 100000.0f;
    camera->is_orthographic = false;
    add_component(world, player_camera, Position);
    add_component(world, player_camera, Rotation);
    add_component(world, player_camera, Euler_Rotation);

    Entity_Handle test_sphere = spawn_entity(world);
    add_component(world, test_sphere, Local_To_World);
    pos = add_component(world, test_sphere, Position);
    pos->v = glm::vec3(50.0f, 1.0f, 50.0f);
    add_component(world, test_sphere, Rotation);
    auto renderer = add_component(world, test_sphere, Mesh_Renderer);
    renderer->mesh = scene->mesh_sphere;
    renderer->material = snow_material;

    Material sky_material = {};
    sky_material.type = Material_Type_Sky;
    sky_material.Sky.map = &scene->texture_sky;
    sky_material.Sky.shader = &scene->sky_shader;
                                //
    Entity_Handle sky = spawn_entity(world);
    renderer = add_component(world, sky, Mesh_Renderer);
    renderer->mesh = scene->mesh_sky;
    renderer->material = sky_material;

    Entity_Handle terrain = spawn_entity(world);
    renderer = add_component(world, terrain, Mesh_Renderer);
    renderer->mesh = scene->mesh_terrain;
    renderer->material = snow_ground_material;

    // // Create a snowman
    // Entity_Handle snowman_base = spawn_entity(world);
    // {
    //     set_name(world, snowman_base, "Snowman");
    //     set_mesh(world, snowman_base, scene->mesh_sphere, snow_material);

    //     Entity_Handle snowman_middle = spawn_entity(world);
    //     set_name(world,   snowman_middle, "Snowman Middle");
    //     set_parent(world, snowman_middle, snowman_base);
    //     set_mesh(world,   snowman_middle, scene->mesh_sphere, snow_material);
    //     translate(world,  snowman_middle, glm::vec3(0.0f, 1.3f, 0.0f));
    //     scale(world,      snowman_middle, glm::vec3(0.7f));

    //     // Create simple solid color materials
    //     Material carrot_material = snow_material;
    //     carrot_material.Phong.diffuse_color = glm::vec3(1.0f, 0.5f, 0.1f);
    //     carrot_material.Phong.diffuse_map = &scene->texture_default;
    //     carrot_material.Phong.specular_map = &scene->texture_default;
    //     carrot_material.Phong.shininess = 1.0f;
    //     Material wood_material = carrot_material;
    //     wood_material.Phong.diffuse_color = glm::vec3(0.4f, 0.15f, 0.075f);

    //     Entity_Handle snowman_arm_l = spawn_entity(world);
    //     set_name(world,   snowman_arm_l, "Snowman Arm L");
    //     set_parent(world, snowman_arm_l, snowman_middle);
    //     set_mesh(world,   snowman_arm_l, scene->mesh_cylinder, wood_material);
    //     translate(world,  snowman_arm_l, glm::vec3(-2.8f, -0.5f, 0.0f));
    //     rotate(world,     snowman_arm_l, glm::vec3(0.0f, 0.0f, half_pi+0.2f));
    //     scale(world,      snowman_arm_l, glm::vec3(0.3f, 1.0f, 0.3f));

    //     Entity_Handle snowman_arm_r = spawn_entity(world);
    //     set_name(world,   snowman_arm_r, "Snowman Arm R");
    //     set_parent(world, snowman_arm_r, snowman_middle);
    //     set_mesh(world,   snowman_arm_r, scene->mesh_cylinder, wood_material);
    //     translate(world,  snowman_arm_r, glm::vec3(0.8f, 0.0f, 0.0f));
    //     rotate(world,     snowman_arm_r, glm::vec3(0.0f, 0.0f, half_pi-0.2f));
    //     scale(world,      snowman_arm_r, glm::vec3(0.3f, 1.0f, 0.3f));

    //     Entity_Handle snowman_head = spawn_entity(world);
    //     set_name(world,   snowman_head, "Snowman Head");
    //     set_parent(world, snowman_head, snowman_middle);
    //     set_mesh(world,   snowman_head, scene->mesh_sphere, snow_material);
    //     translate(world,  snowman_head, glm::vec3(0.0f, 1.3f, 0.0f));
    //     scale(world,      snowman_head, glm::vec3(0.7f));

    //     Entity_Handle snowman_carrot = spawn_entity(world);
    //     set_name(world,   snowman_carrot, "Snowman Carrot");
    //     set_mesh(world,   snowman_carrot, scene->mesh_cone, carrot_material);
    //     set_parent(world, snowman_carrot, snowman_head);
    //     rotate(world,     snowman_carrot, glm::vec3(half_pi, 0.0f, 0.0f));
    // }

    // // Create copies of the snowman
    // for (int i = 0; i < 30; i++) {
    //     Entity_Handle snowman_copy = copy_entity(world, snowman_base);

    //     Transform tr = {};
    //     f32 scale = comp(rng)*0.8f+0.2f;
    //     glm::vec3 pos(dist(rng), 0.0f, dist(rng));
    //     pos.y = sample_point_at(&scene->terrain, pos.x, pos.z) + 0.8f*scale;
    //     tr.local_position = pos;
    //     tr.local_scale = glm::vec3(scale);
    //     tr.is_dirty = true;
    //     update_transform(&tr);
    //     set_local_transform(world, snowman_copy, tr.matrix);
    // }

    // Setup main systems
    push_transform_systems(scene->main_systems);
    push_camera_systems(scene->main_systems);

    // Setup rendering pipeline
    push_mesh_renderer_system(scene->rendering_pipeline, &scene->player_camera);

    scene->sensitivity = 0.01f;

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

    // Update player and its camera
    control_player(&scene->world,
                   scene->player,
                   scene->player_camera,
                   &window->input,
                   &scene->terrain,
                   scene->sensitivity);

    // Make the players camera fit the entire window
    glm::vec4 viewport(0.0f, 0.0f, (f32) window->width, (f32) window->height);
    auto camera = get_component(&scene->world, scene->player_camera, Camera);
    if (camera) {
        if (window->width != 0 && window->height != 0) {
            camera->aspect = ((f32) window->width)/((f32) window->height);
        }
        camera->viewport = viewport;
    }

    // Use wireframe if enabled
    if (scene->enable_wireframe) {
        glLineWidth(2);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Update the world
    update_systems(&scene->world, scene->main_systems, 0.0f); // TODO(alexander): delta time!!!

    // Render the world
    begin_frame(scene->world.clear_color, viewport, true);
    update_systems(&scene->world, scene->rendering_pipeline, 0.0f);
    end_frame();    

    ImGui::Begin("Lab 4 - Simple World", &scene->show_gui, ImVec2(280, 150), ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Light Setup:");
    ImGui::DragFloat3("Position", &scene->world.light.pos.x);
    ImGui::ColorEdit3("Ambient", &scene->world.light.ambient.x);
    ImGui::ColorEdit3("Diffuse", &scene->world.light.diffuse.x);
    ImGui::ColorEdit3("Specular", &scene->world.light.specular.x);

    ImGui::Text("Fog:");
    ImGui::ColorEdit3("Color", &scene->world.clear_color.x);
    ImGui::SliderFloat("Density", &scene->world.fog_density, 0.01f, 0.5f);
    ImGui::SliderFloat("Gradient", &scene->world.fog_gradient, 1.0f, 10.0f);
    ImGui::Checkbox("Wireframe mode", &scene->enable_wireframe);
    ImGui::End();
}
