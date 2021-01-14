
/***************************************************************************
 * Lab assignment 3 - Alexander Mennborg
 * Basic 3D Graphics using OpenGL
 ***************************************************************************/

struct Basic_3D_Graphics_Scene {
    Basic_Shader basic_shader;

    World world;
    std::vector<System> main_systems;
    std::vector<System> rendering_pipeline;
    Entity_Handle movable_cube;
    Entity_Handle camera;

    float light_attenuation;
    float light_intensity;

    bool show_gui;

    bool is_initialized;
};

struct Movable_Cube_Controller {
    Input* input;
};

REGISTER_COMPONENT(Movable_Cube_Controller);

DEF_SYSTEM(movable_cube_controller_system) {
    auto controller = (Movable_Cube_Controller*) components[0];
    auto pos = (Position*) components[1];

    float speed = 0.02f;
    if (controller->input->shift_key.ended_down) {
        speed = 0.08f;
    }

    if (pos) {
        if (controller->input->a_key.ended_down) pos->v.x -= speed;
        if (controller->input->d_key.ended_down) pos->v.x += speed;
        if (controller->input->w_key.ended_down) pos->v.y += speed;
        if (controller->input->s_key.ended_down) pos->v.y -= speed;
        if (controller->input->c_key.ended_down) pos->v.z += speed;
        if (controller->input->e_key.ended_down) pos->v.z -= speed;
    }
};

static bool
initialize_scene(Basic_3D_Graphics_Scene* scene, Window* window) {
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
        Entity_Handle entity = spawn_entity(world);
        add_component(world, entity, Local_To_World);
        auto pos = add_component(world, entity, Position);
        pos->v = glm::vec3(dist(rng)*3.0f, dist(rng)*2.0f, dist(rng) - 6.0f);
        auto renderer = add_component(world, entity, Mesh_Renderer);
        renderer->mesh = cuboid_mesh;
        renderer->material = material;
    }

    // Create the movable cube entity
    Entity_Handle movable_cube = spawn_entity(world);
    scene->movable_cube = movable_cube;
    add_component(world, movable_cube, Local_To_World);
    auto controller = add_component(world, movable_cube, Movable_Cube_Controller);
    controller->input = &window->input;
    auto pos = add_component(world, movable_cube, Position);
    pos->v = glm::vec3(0.0f, 0.0f, -1.0f);

    material.Basic.color = green_color;
    auto renderer = add_component(world, movable_cube, Mesh_Renderer);
    renderer->mesh = cuboid_mesh;
    renderer->material = material;

    // Create camera entity
    Entity_Handle camera = spawn_entity(world);
    scene->camera = camera;
    auto camera_component = add_component(world, camera, Camera);
    camera_component->fov = half_pi;
    camera_component->near = 0.01f;
    camera_component->far = 100000.0f;
    camera_component->is_orthographic = false;
    camera_component->window = window;
    pos = add_component(world, camera, Position);
    pos->v = glm::vec3(0.0f);
    add_component(world, camera, Rotation);
    add_component(world, camera, Euler_Rotation);

    // Setup main systems
    System cube_controller = {};
    cube_controller.on_update = &movable_cube_controller_system;
    use_component(cube_controller, Movable_Cube_Controller);
    use_component(cube_controller, Position);
    push_system(scene->main_systems, cube_controller);

    push_transform_systems(scene->main_systems);
    push_camera_systems(scene->main_systems);

    // Setup rendering pipeline
    push_mesh_renderer_system(scene->rendering_pipeline, &scene->camera);

    // Scene and world properties
    world->renderer.light_intensity = 0.4f;
    world->renderer.light_attenuation = 0.03f;

    scene->is_initialized = true;
    return true;
}

void
render_scene(Basic_3D_Graphics_Scene* scene, Window* window, f32 dt) {
    auto camera = get_component(&scene->world, scene->camera, Camera);
    begin_frame(primary_bg_color, camera->viewport, true, &scene->world.renderer);
    update_systems(&scene->world, scene->rendering_pipeline, dt);
    end_frame();

    // ImGui
    ImGui::Begin("Lab 3 - Basic 3D Graphics", &scene->show_gui);
    ImGui::Text("Light:");
    ImGui::SliderFloat("Intensity", &scene->world.renderer.light_intensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Attenuation", &scene->world.renderer.light_attenuation, 0.001f, 0.2f);
    ImGui::End();
}
