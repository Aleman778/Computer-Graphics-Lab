
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

    Phong_Shader phong_shader;
    Sky_Shader sky_shader;

    Texture texture_default;
    Texture texture_snow_01_diffuse;
    Texture texture_snow_01_specular;
    Texture texture_snow_02_diffuse;
    Texture texture_snow_02_specular;
    Texture texture_metal_diffuse;
    Texture texture_metal_specular;
    Texture texture_sky;

    Height_Map terrain;

    bool enable_wireframe;
    bool show_gui;

    bool is_initialized;
};

struct Player_Controller {
    Entity_Handle camera;
    Input* input;
    Height_Map* terrain;
    f32 sensitivity;
};

REGISTER_COMPONENT(Player_Controller);

DEF_SYSTEM(player_controller_system) {
    auto pc  = (Player_Controller*) components[0];
    auto pos = (Position*)          components[1];
    auto rot = (Euler_Rotation*)    components[2];

    // Looking around, whenever mouse is locked, left click in window to lock, press escape to unlock.
    if (pc->input->mouse_locked) {
        f32 delta_x = pc->input->mouse_delta_x;
        f32 delta_y = pc->input->mouse_delta_y;

        bool is_dirty = false;
        if (delta_x > 0.1f || delta_x < -0.1f) {
            rot->v.x += delta_x * pc->sensitivity;
            is_dirty = true;
        }

        if (delta_y > 0.1f || delta_y < -0.1f) {
            rot->v.y += delta_y * pc->sensitivity;
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
    if (pc->input->shift_key.ended_down) {
        speed = 0.08f;
    }

    glm::vec3 forward(cos(rot->v.x + pi + half_pi)*speed, 0.0f, sin(rot->v.x + pi + half_pi)*speed);
    glm::vec3 right(cos(rot->v.x + pi)*speed, 0.0f, sin(rot->v.x + pi)*speed);
    if (pc->input->w_key.ended_down) pos->v += forward;
    if (pc->input->a_key.ended_down) pos->v += right;
    if (pc->input->s_key.ended_down) pos->v -= forward;
    if (pc->input->d_key.ended_down) pos->v -= right;

    // Enable/ disable mouse locking
    if (was_pressed(&pc->input->left_mb)) pc->input->mouse_locked = true;
    if (was_pressed(&pc->input->escape_key)) pc->input->mouse_locked = false;

    // Invisible wall collision
    if (pos->v.x <  0.0f) pos->v.x =   0.0f;
    if (pos->v.x > 99.5f) pos->v.x = 99.5f;
    if (pos->v.z <  0.0f) pos->v.z =   0.0f;
    if (pos->v.z > 99.5f) pos->v.z = 99.5f;

    // Gravity
    pos->v.y -= 0.098f; // TODO(alexander): add to velocity

    // Terrain collision
    f32 terrain_height = sample_point_at(pc->terrain, pos->v.x, pos->v.z) + 1.8f;
    if (pos->v.y < terrain_height) {
        pos->v.y = terrain_height;
    }

    // Update the camera to players position
    auto camera_pos = get_component(world, pc->camera, Position);
    auto camera_rot = get_component(world, pc->camera, Euler_Rotation);

    if (camera_pos) camera_pos->v = pos->v;
    if (camera_rot) camera_rot->v = rot->v;
}

// NOTE(alexander): optimized entity for static meshes
static Entity_Handle
spawn_static_mesh_entity(World* world,
                         const Material& material,
                         const Mesh& mesh,
                         Entity_Handle* parent_handle,
                         glm::vec3 pos,
                         glm::vec3 rot=glm::vec3(0.0f, 0.0f, 0.0f),
                         glm::vec3 scl=glm::vec3(1.0f, 1.0f, 1.0f)) {
    Entity_Handle entity = spawn_entity(world);

    glm::quat rot_x(glm::vec3(0.0f, rot.x, 0.0f));
    glm::quat rot_y(glm::vec3(rot.y, 0.0f, 0.0f));
    glm::quat rot_z(glm::vec3(0.0f, 0.0f, rot.z));
    glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 R = glm::toMat4(rot_z * rot_y * rot_x);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scl);

    if (parent_handle) {
        add_component(world, entity, Local_To_World);
        auto local_to_parent = add_component(world, entity, Local_To_Parent);
        local_to_parent->m = T * R * S;
    } else {
        auto local_to_world = add_component(world, entity, Local_To_World);
        local_to_world->m = T * R * S;
    }

    auto renderer = add_component(world, entity, Mesh_Renderer);
    renderer->material = material;
    renderer->mesh = mesh;

    if (parent_handle) {
        auto parent = add_component(world, entity, Parent);
        parent->handle = *parent_handle;
    }

    return entity;
}

static bool
initialize_scene(Simple_World_Scene* scene, Window* window) {
    scene->phong_shader = compile_phong_shader();
    scene->sky_shader = compile_sky_shader();

    // Create some basic meshes to build from
    Mesh mesh_cube;
    Mesh mesh_terrain;
    Mesh mesh_sphere;
    Mesh mesh_sky;
    Mesh mesh_cylinder;
    Mesh mesh_cone;
    Mesh mesh_conical_frustum;

    {
        Mesh_Builder mb = {};
        push_cuboid_mesh(&mb, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        mesh_cube = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        scene->terrain = generate_terrain_mesh(&mb, 100.0f, 100.f, 200, 200, 8, 0.6f, 2.0f, -1.0f);
        for (int i = 0; i < mb.vertices.size(); i++) {
            mb.vertices[i].texcoord *= 0.3f;
        }
        mesh_terrain = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_sphere(&mb, glm::vec3(0.0f), 1.0f);
        mesh_sphere = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_sphere(&mb, glm::vec3(0.0f), 100.0f);
        mesh_sky = create_mesh_from_builder(&mb);
        mesh_sky.is_two_sided = true; // since we are always inside the sky dome
    }

    {
        Mesh_Builder mb = {};
        push_cylinder_triangles(&mb, glm::vec3(0.0f), 0.5f, 1.0f);
        mesh_cylinder = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_cone_triangles(&mb, glm::vec3(0.0f), 0.5f, 1.0f);
        mesh_cone = create_mesh_from_builder(&mb);
    }

    {
        Mesh_Builder mb = {};
        push_conical_frustum_triangles(&mb, glm::vec3(0.0f), 0.5f, 0.25f, 1.0f);
        mesh_conical_frustum = create_mesh_from_builder(&mb);
    }

    // Load textures
    scene->texture_default          = generate_white_2d_texture();
    scene->texture_snow_01_diffuse  = load_texture_2d_from_file("snow_01_diffuse.png");
    scene->texture_snow_01_specular = load_texture_2d_from_file("snow_01_specular.png");
    scene->texture_snow_02_diffuse  = load_texture_2d_from_file("snow_02_diffuse.png");
    scene->texture_snow_02_specular = load_texture_2d_from_file("snow_02_specular.png");
    scene->texture_metal_diffuse    = load_texture_2d_from_file("green_metal_rust_diffuse.png");
    scene->texture_metal_specular   = load_texture_2d_from_file("green_metal_rust_specular.png");
    scene->texture_sky              = load_texture_2d_from_file("satara_night_no_lamps_2k.hdr");
    // scene->texture_sky        = load_texture_2d_from_file("winter_lake_01_1k.hdr");

    // Setup random number generator
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
    snow_ground_material.Phong.shininess = 2.0f;
    snow_ground_material.Phong.shader = &scene->phong_shader;

    Material snow_material = snow_ground_material;
    snow_material.Phong.diffuse = &scene->texture_snow_02_diffuse;
    snow_material.Phong.specular = &scene->texture_snow_02_specular;

    Material metal_material = snow_material;
    metal_material.Phong.diffuse = &scene->texture_metal_diffuse;
    metal_material.Phong.specular = &scene->texture_metal_specular;
    metal_material.Phong.shininess = 32.0f;

    Material carrot_material = snow_material;
    carrot_material.Phong.color = glm::vec3(1.0f, 0.5f, 0.1f);
    carrot_material.Phong.diffuse = &scene->texture_default;
    carrot_material.Phong.specular = &scene->texture_default;
    carrot_material.Phong.shininess = 1.0f;

    Material wood_material = carrot_material;
    wood_material.Phong.color = glm::vec3(0.4f, 0.15f, 0.075f);

    // Setup the world
    World* world = &scene->world;

    // Scene and world properties
    world->renderer.fog_color = glm::vec4(0.01f, 0.01f, 0.01f, 1.0f);
    world->renderer.fog_density = 0.05f;
    world->renderer.fog_gradient = 2.0f;
    world->renderer.directional_light.direction = glm::vec3(0.7f, -1.0f, 0.0f);
    world->renderer.directional_light.ambient   = glm::vec3(0.003f, 0.003f, 0.005f);
    world->renderer.directional_light.diffuse   = glm::vec3(0.03f, 0.03f, 0.05f);
    world->renderer.directional_light.specular  = glm::vec3(0.02f, 0.02f, 0.04f);
    scene->enable_wireframe = false;

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

    // Player
    Entity_Handle player = spawn_entity(world);
    scene->player = player;
    auto pc = add_component(world, player, Player_Controller);
    pc->camera = player_camera;
    pc->input = &window->input;
    pc->terrain = &scene->terrain;
    pc->sensitivity = 0.01f;
    add_component(world, player, Local_To_World);
    auto pos = add_component(world, player, Position);
    pos->v = glm::vec3(50.0f, -50.0f, 50.0f);
    add_component(world, player, Rotation);
    add_component(world, player, Euler_Rotation);

    Material sky_material = {};
    sky_material.type = Material_Type_Sky;
    sky_material.Sky.map = &scene->texture_sky;
    sky_material.Sky.shader = &scene->sky_shader;

    Entity_Handle sky = spawn_entity(world);
    auto renderer = add_component(world, sky, Mesh_Renderer);
    renderer->mesh = mesh_sky;
    renderer->material = sky_material;

    Entity_Handle terrain = spawn_entity(world);
    renderer = add_component(world, terrain, Mesh_Renderer);
    renderer->mesh = mesh_terrain;
    renderer->material = snow_ground_material;

    // Create many snowmen
    for (int i = 0; i < 30; i++) {
        glm::vec3 p(dist(rng), 0.0f, dist(rng));
        glm::vec3 scale(comp(rng)*0.8f + 0.2f);
        p.y = sample_point_at(&scene->terrain, p.x, p.z) + scale.x;
        auto snowman_base = spawn_static_mesh_entity(world, snow_material, mesh_sphere, NULL,
                                                     p,
                                                     glm::vec3(comp(rng)*two_pi, 0.0f, 0.0f),
                                                     scale);

        auto snowman_middle = spawn_static_mesh_entity(world, snow_material, mesh_sphere, &snowman_base,
                                                       glm::vec3(0.0f, 1.3f, 0.0f),
                                                       glm::vec3(0.0f),
                                                       glm::vec3(0.7f));

        auto snowman_head = spawn_static_mesh_entity(world, snow_material, mesh_sphere, &snowman_middle,
                                                     glm::vec3(0.0f, 1.3f, 0.0f),
                                                     glm::vec3(0.0f),
                                                     glm::vec3(0.7f));

        auto snowman_arm_l = spawn_static_mesh_entity(world, wood_material, mesh_cylinder, &snowman_middle,
                                                      glm::vec3(-0.5f, 0.25f, 0.0f),
                                                      glm::vec3(0.0f, 0.0f, half_pi+0.2f),
                                                      glm::vec3(0.3f, 2.0f, 0.3f));

        auto snowman_arm_r = spawn_static_mesh_entity(world, wood_material, mesh_cylinder, &snowman_middle,
                                                      glm::vec3(2.5f, -0.12f, 0.0f),
                                                      glm::vec3(0.0f, 0.0f, half_pi-0.2f),
                                                      glm::vec3(0.3f, 2.0f, 0.3f));

        auto snowman_carrot = spawn_static_mesh_entity(world, carrot_material, mesh_cone, &snowman_head,
                                                       glm::vec3(0.0f),
                                                       glm::vec3(0.0f, half_pi, 0.0f),
                                                       glm::vec3(1.0f, 2.0f, 1.0f));
    }

    // Create lamp posts
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        auto p = glm::vec3(50.0f + 20.0f*i, 0.0f, 45.0f);
        p.y = sample_point_at(&scene->terrain, p.x, p.z) - 0.2f;
        auto lamp_post_base = spawn_static_mesh_entity(world, metal_material, mesh_cylinder, NULL,
                                                       p, glm::vec3(0.0f), glm::vec3(0.4f, 1.0f, 0.4f));
        spawn_static_mesh_entity(world, metal_material, mesh_conical_frustum, &lamp_post_base,
                                 glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 0.1f, 1.0f));

        auto lamp_post_middle = spawn_static_mesh_entity(world, metal_material, mesh_cylinder, &lamp_post_base,
                                                         glm::vec3(0.0f, 1.1f, 0.0f),
                                                         glm::vec3(0.0f),
                                                         glm::vec3(0.5f, 1.5f, 0.5f));

        spawn_static_mesh_entity(world, metal_material, mesh_conical_frustum, &lamp_post_middle,
                                 glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f, 0.05f, 1.0f));

        auto lamp_post_top = spawn_static_mesh_entity(world, metal_material, mesh_cylinder, &lamp_post_middle,
                                                      glm::vec3(0.0f, 1.05f, 0.0f),
                                                      glm::vec3(0.0f),
                                                      glm::vec3(0.5f, 1.5f, 0.5f));

        auto lamp_post_head_base = spawn_static_mesh_entity(world, metal_material, mesh_cone, &lamp_post_top,
                                                            glm::vec3(0.0f, 1.0f, 0.0f),
                                                            glm::vec3(0.0f, -pi, 0.0f),
                                                            glm::vec3(3.0f, 0.05f, 3.0f));

        auto lamp_post_head_top = spawn_static_mesh_entity(world, metal_material, mesh_cone, &lamp_post_head_base,
                                                           glm::vec3(0.0f, -4.0f, 0.0f),
                                                           glm::vec3(0.0f, pi, 0.0f),
                                                           glm::vec3(1.5f, 1.0f, 1.5f));

        spawn_static_mesh_entity(world, metal_material, mesh_cube, &lamp_post_head_base,
                                 glm::vec3(0.6f, -2.0f, 0.0f),
                                 glm::vec3(0.0f, pi, 0.08f),
                                 glm::vec3(0.1f, 8.0f, 0.4f));

        spawn_static_mesh_entity(world, metal_material, mesh_cube, &lamp_post_head_base,
                                 glm::vec3(-0.6f, -2.0f, 0.0f),
                                 glm::vec3(0.0f, pi, -0.08f),
                                 glm::vec3(0.1f, 8.0f, 0.4f));

        spawn_static_mesh_entity(world, metal_material, mesh_cube, &lamp_post_head_base,
                                 glm::vec3(0.0f, -2.0f, 0.6f),
                                 glm::vec3(0.0f, pi-0.08f, 0.0f),
                                 glm::vec3(0.4f, 8.0f, 0.1f));

        spawn_static_mesh_entity(world, metal_material, mesh_cube, &lamp_post_head_base,
                                 glm::vec3(0.0f, -2.0f, -0.6f),
                                 glm::vec3(0.0f, pi+0.08f, 0.0f),
                                 glm::vec3(0.4f, 8.0f, 0.1f));

        world->renderer.point_lights[i].position  = glm::vec3(p.x, p.y + 5.2f, p.z);
        world->renderer.point_lights[i].constant  = 0.3f;
        world->renderer.point_lights[i].linear    = 0.09f;
        world->renderer.point_lights[i].quadratic = 0.032f;
        world->renderer.point_lights[i].ambient   = glm::vec3(0.1f, 0.1f, 0.1f);
        world->renderer.point_lights[i].diffuse   = glm::vec3(1.0f, 1.0f, 1.0f);
        world->renderer.point_lights[i].specular  = glm::vec3(1.0f, 1.0f, 1.0f);
    }

    // Setup main systems
    System controller = {};
    controller.on_update = &player_controller_system;
    use_component(controller, Player_Controller);
    use_component(controller, Position);
    use_component(controller, Euler_Rotation);
    push_system(scene->main_systems, controller);

    push_hierarchical_transform_systems(scene->main_systems);

    push_camera_systems(scene->main_systems);

    // Setup rendering pipeline
    push_mesh_renderer_system(scene->rendering_pipeline, &scene->player_camera);

    scene->is_initialized = true;
    return true;
}

void
update_scene(Simple_World_Scene* scene, Window* window, float dt) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene, window)) {
            is_running = false;
            return;
        }
    }

    // Make the players camera fit the entire window
    auto camera = get_component(&scene->world, scene->player_camera, Camera);
    if (camera) {
        if (window->width != 0 && window->height != 0) {
            camera->aspect = ((f32) window->width)/((f32) window->height);
        }
        camera->viewport = glm::vec4(0.0f, 0.0f, (f32) window->width, (f32) window->height);
    }

    // Update the world
    update_systems(&scene->world, scene->main_systems, dt);
}

void
render_scene(Simple_World_Scene* scene, Window* window, float dt) {
    // Use wireframe if enabled
    if (scene->enable_wireframe) {
        glLineWidth(2);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    World* world = &scene->world;

    // Render the world
    auto camera = get_component(world, scene->player_camera, Camera);
    begin_frame(world->renderer.fog_color, camera->viewport, true);
    update_systems(world, scene->rendering_pipeline, dt);
    end_frame();

    // ImGui
    ImGui::Begin("Lab 4 - Simple World", &scene->show_gui, ImVec2(280, 450), ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Lighting:");
    static int curr_light = 0;
    static const char* light_names[] = {
        "Directional Light",
        "Point Light 1",
        "Point Light 2",
        "Point Light 3",
        "Point Light 4"};
    ImGui::Combo("", &curr_light, light_names, MAX_POINT_LIGHTS + 1);
    if (curr_light == 0) {
        ImGui::DragFloat3("Direction",  &world->renderer.directional_light.direction.x, 0.01f);
        ImGui::ColorEdit3("Ambient",    &world->renderer.directional_light.ambient.x);
        ImGui::ColorEdit3("Diffuse",    &world->renderer.directional_light.diffuse.x);
        ImGui::ColorEdit3("Specular",   &world->renderer.directional_light.specular.x);
    } else {
        ImGui::DragFloat3("Position", &world->renderer.point_lights[curr_light - 1].position.x, 0.1f);
        ImGui::DragFloat("Constant",  &world->renderer.point_lights[curr_light - 1].constant, 0.001f);
        ImGui::DragFloat("Linear",    &world->renderer.point_lights[curr_light - 1].linear, 0.001f);
        ImGui::DragFloat("Quadratic", &world->renderer.point_lights[curr_light - 1].quadratic, 0.001f);
        ImGui::ColorEdit3("Ambient",  &world->renderer.point_lights[curr_light - 1].ambient.x);
        ImGui::ColorEdit3("Diffuse",  &world->renderer.point_lights[curr_light - 1].diffuse.x);
        ImGui::ColorEdit3("Specular", &world->renderer.point_lights[curr_light - 1].specular.x);
    }

    ImGui::Text("Fog:");
    ImGui::ColorEdit3("Color", &world->renderer.fog_color.x);
    ImGui::SliderFloat("Density", &world->renderer.fog_density, 0.01f, 0.5f);
    ImGui::SliderFloat("Gradient", &world->renderer.fog_gradient, 1.0f, 10.0f);

    ImGui::Text("Miscellaneous:");
    ImGui::Checkbox("Wireframe mode", &scene->enable_wireframe);
    ImGui::End();
}
