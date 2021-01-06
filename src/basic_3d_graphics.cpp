
/***************************************************************************
 * Lab assignment 3 - Alexander Mennborg
 * Basic 3D Graphics using OpenGL
 ***************************************************************************/

struct Basic_3D_Graphics_Scene {
    Basic_Shader basic_shader;

    World world;
    Entity movable_cube;
    Entity camera;

    glm::vec3 movable_cube_position;
    
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

    // Basic material
    Material material = {};
    material.type = Material_Type_Basic;
    material.Basic.shader = &scene->basic_shader;
    material.Basic.color = primary_fg_color;

    // Create all the graphics nodes
    for (int i = 0; i < 100; i++) {
        glm::vec3 pos(dist(rng)*3.0f, dist(rng)*2.0f, dist(rng) - 4.5f);
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos);

        Entity entity = spawn_entity(&scene->world);
        set_local_transform(&scene->world, entity, transform);
        set_mesh(&scene->world, entity, cuboid_mesh, material);
    }

    // Create the movable cube entity
    scene->movable_cube_position = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), scene->movable_cube_position);
    material.Basic.color = green_color;
    scene->movable_cube = spawn_entity(&scene->world);
    set_local_transform(&scene->world, scene->movable_cube, transform);
    set_mesh(&scene->world, scene->movable_cube, cuboid_mesh, material);

    // Create camera entity
    scene->camera = spawn_entity(&scene->world);
    scene->world.main_camera = scene->camera;
    set_perspective(&scene->world, scene->camera);

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

    if (window->input.a_key.ended_down) scene->movable_cube_position.x -= speed;
    if (window->input.d_key.ended_down) scene->movable_cube_position.x += speed;
    if (window->input.w_key.ended_down) scene->movable_cube_position.y += speed;
    if (window->input.s_key.ended_down) scene->movable_cube_position.y -= speed;
    if (window->input.c_key.ended_down) scene->movable_cube_position.z += speed;
    if (window->input.e_key.ended_down) scene->movable_cube_position.z -= speed;

    adjust_camera_to_fill_window(&scene->world, scene->camera, window->width, window->height);

    render_world(&scene->world);

    // ImGui
    ImGui::Begin("Lab 3 - Basic 3D Graphics", &scene->show_gui, ImVec2(280, 150), ImGuiWindowFlags_NoSavedSettings);
    ImGui::SliderFloat("Light intensity", &scene->light_intensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Light attenuation", &scene->light_attenuation, 0.001f, 0.2f);
    ImGui::End();

}
