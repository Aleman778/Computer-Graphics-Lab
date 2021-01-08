
/***************************************************************************
 * Lab assignment 3 - Alexander Mennborg
 * Basic 3D Graphics using OpenGL
 ***************************************************************************/

struct Basic_3D_Graphics_Scene {
    Basic_Shader basic_shader;

    World world;
    Entity_Handle movable_cube;
    Entity_Handle camera;

    float light_attenuation;
    float light_intensity;

    bool show_gui;

    bool is_initialized;
};

static bool
initialize_scene(Basic_3D_Graphics_Scene* scene) {
    scene->basic_shader = compile_basic_shader();

    Mesh_Builder mb = {};
    push_cuboid_mesh(&mb, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f));
    Mesh cuboid_mesh = create_mesh_from_builder(&mb);

    // Setup random number generator for generating cuboid positions
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(-5.0f, 5.0f);

    // Setup a basic material
    Material material = {};
    material.type = Material_Type_Basic;
    material.Basic.shader = &scene->basic_shader;
    material.Basic.color = primary_fg_color;

    // Setup the world
    World* world = &scene->world;

    // Create all the graphics nodes
    for (int i = 0; i < 100; i++) {
        Entity_Handle entity = spawn_entity(&scene->world);
        add_component(world, entity, Local_To_World);
        auto pos = add_component(world, entity, Position);
        pos->v = glm::vec3(dist(rng)*3.0f, dist(rng)*2.0f, dist(rng) - 4.5f);
        
        auto renderer = add_component(world, entity, Mesh_Renderer);
        renderer->mesh = cuboid_mesh;
        renderer->material = material;
    }

    // Create the movable cube entity
    Entity_Handle movable_cube = spawn_entity(&scene->world);
    scene->movable_cube = movable_cube;
    material.Basic.color = green_color;
    add_component(world, movable_cube, Local_To_World);
    auto pos = add_component(world, movable_cube, Position);
    pos->v = glm::vec3(0.0f, 0.0f, -1.0f);
        
    auto renderer = add_component(world, movable_cube, Mesh_Renderer);
    renderer->mesh = cuboid_mesh;
    renderer->material = material;

    // Create camera entity
    Entity_Handle camera = spawn_entity(world);
    scene->camera = camera;
    world->main_camera = camera;
    auto camera_component = add_component(world, camera, Camera);
    camera_component->fov = half_pi;
    camera_component->near = 0.01f;
    camera_component->far = 100000.0f;
    add_component(world, camera, Position);

    // Scene properties
    scene->light_intensity = 0.4f;
    scene->light_attenuation = 0.03f;

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

    // Control the movable cuboid
    float speed = 0.02f;
    if (window->input.shift_key.ended_down) {
        speed = 0.08f;
    }

    // TODO(alexander): this could be moved into a system and component
    auto cube_pos = get_component(&scene->world, scene->movable_cube, Position);
    if (cube_pos) {
        if (window->input.a_key.ended_down) cube_pos->v.x -= speed;
        if (window->input.d_key.ended_down) cube_pos->v.x += speed;
        if (window->input.w_key.ended_down) cube_pos->v.y += speed;
        if (window->input.s_key.ended_down) cube_pos->v.y -= speed;
        if (window->input.c_key.ended_down) cube_pos->v.z += speed;
        if (window->input.e_key.ended_down) cube_pos->v.z -= speed;
    }
    
    // Make the players camera fit the entire window
    auto camera = get_component(&scene->world, scene->camera, Camera);
    if (camera) {
        if (window->width != 0 && window->height != 0) {
            camera->aspect = ((f32) window->width)/((f32) window->height);
        }
        camera->viewport = glm::vec4(0, 0, (f32) window->width, (f32) window->height);
    }

    update_systems(&scene->world, 0.0f); // TODO(alexander): delta time!!!

    render_world(&scene->world);

    // ImGui
    ImGui::Begin("Lab 3 - Basic 3D Graphics", &scene->show_gui, ImVec2(280, 150), ImGuiWindowFlags_NoSavedSettings);
    ImGui::SliderFloat("Light intensity", &scene->light_intensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Light attenuation", &scene->light_attenuation, 0.001f, 0.2f);
    ImGui::End();

}
