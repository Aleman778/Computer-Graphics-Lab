
static constexpr char* default_entity_name = "Entity";

struct World_Editor {
    World* world;
    Entity_Handle editor_camera;
    Entity_Handle selected;
    std::vector<System> main_systems;
    std::vector<System> rendering_pipeline;

    ImGuizmo::OPERATION guizmo_operation;
    ImGuizmo::MODE guizmo_mode;

    bool show_hierarchy;
    bool show_inspector;

    bool is_initialized;
};

struct Editor_Camera_Controller {
    Input* input;
    f32 speed;
    f32 mouse_sensitivity;
};

REGISTER_COMPONENT(Editor_Camera_Controller);

DEF_SYSTEM(editor_camera_controller_system) {
    auto controller = (Editor_Camera_Controller*) components[0];
    auto pos = (Position*) components[1];
    auto rot = (Euler_Rotation*) components[2];

    Input* input = controller->input;

    // Rotate camera
    if (input->right_mb.ended_down) {
        f32 delta_x = input->mouse_delta_x;
        f32 delta_y = input->mouse_delta_y;

        bool is_dirty = false;
        if (delta_x > 0.1f || delta_x < -0.1f) {
            rot->v.x += delta_x * controller->mouse_sensitivity;
            is_dirty = true;
        }

        if (delta_y > 0.1f || delta_y < -0.1f) {
            rot->v.y += delta_y * controller->mouse_sensitivity;
            if (rot->v.y < -1.5f) {
                rot->v.y = -1.5f;
            }

            if (rot->v.y > 1.5f) {
                rot->v.y = 1.5f;
            }
        }
    }

    // Translate
    f32 speed = controller->speed;
    glm::vec3 forward(cos(rot->v.x + pi + half_pi), cos(rot->v.y + half_pi), sin(rot->v.x + pi + half_pi));
    glm::vec3 right(cos(rot->v.x + pi), cos(rot->v.y + half_pi - pi), sin(rot->v.x + pi));
    forward = glm::normalize(forward);
    right = glm::normalize(right);
    glm::vec3 up = glm::cross(forward, right);

    if (input->middle_mb.ended_down) {
        pos->v += right * input->mouse_delta_x * controller->mouse_sensitivity;
        pos->v += up * input->mouse_delta_y * controller->mouse_sensitivity;
    }

    pos->v += forward * input->mouse_scroll_y * 60.0f * controller->mouse_sensitivity;
    if (input->w_key.ended_down) pos->v += forward * speed;
    if (input->a_key.ended_down) pos->v += right * speed;
    if (input->s_key.ended_down) pos->v -= forward * speed;
    if (input->d_key.ended_down) pos->v -= right * speed;
}

bool
initialize_world_editor(World_Editor* editor, Window* window) {
    assert(editor->world);
    Entity_Handle editor_camera = spawn_entity(editor->world);
    editor->editor_camera = editor_camera;
    auto camera = add_component(editor->world, editor_camera, Camera);
    camera->fov = half_pi;
    camera->near = 0.01f;
    camera->far = 100000.0f;
    camera->is_orthographic = false;
    camera->window = window;
    auto camera_controller = add_component(editor->world, editor_camera, Editor_Camera_Controller);
    camera_controller->input = &window->input;
    camera_controller->speed = 0.5f;
    camera_controller->mouse_sensitivity = 0.01f;
    auto pos = add_component(editor->world, editor_camera, Position);
    pos->v = glm::vec3(50.0f, 5.0f, 50.0f);
    add_component(editor->world, editor_camera, Rotation);
    add_component(editor->world, editor_camera, Euler_Rotation);

    editor->guizmo_operation = ImGuizmo::TRANSLATE;
    editor->guizmo_mode = ImGuizmo::WORLD;

    // Setup main systems
    System controller = {};
    controller.on_update = &editor_camera_controller_system;
    use_component(controller, Editor_Camera_Controller);
    use_component(controller, Position);
    use_component(controller, Euler_Rotation);
    push_system(editor->main_systems, controller);

    push_hierarchical_transform_systems(editor->main_systems);
    push_camera_systems(editor->main_systems);

    // Setup rendering pipeline
    push_mesh_renderer_system(editor->rendering_pipeline, &editor->editor_camera);

    editor->is_initialized = true;
    return true;
};

void
update_world_editor(World_Editor* editor, Window* window, f32 dt) {
    if (!editor->is_initialized) {
        if (!initialize_world_editor(editor, window)) {
            is_running = false;
            return;
        }
    }

    update_systems(editor->world, editor->main_systems, dt);
};

void
edit_transform(World_Editor* editor, World* world, Camera* camera, Entity* entity) {
    auto local_to_world = get_component(world, entity->handle, Local_To_World);
    auto local_to_parent = get_component(world, entity->handle, Local_To_Parent);
    if (!local_to_world) return;

    ImGuizmo::BeginFrame();

    glm::mat4 world_matrix = local_to_world->m;
    glm::mat4 local_matrix = local_to_parent ? local_to_parent->m : local_to_world->m;

    auto pos = get_component(world, entity->handle, Position);
    auto rot = get_component(world, entity->handle, Rotation);
    auto euler_rot = get_component(world, entity->handle, Euler_Rotation);
    auto scl = get_component(world, entity->handle, Scale);

    if (pos != NULL || rot != NULL || scl != NULL) {
        if (!pos) pos = add_component(world, entity->handle, Position);
        if (!rot) rot = add_component(world, entity->handle, Rotation);
        if (!euler_rot) euler_rot = add_component(world, entity->handle, Euler_Rotation);
        if (!scl) scl = add_component(world, entity->handle, Scale);
    }

    if (ImGui::IsKeyPressed(90)) editor->guizmo_operation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(69)) editor->guizmo_operation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(82)) editor->guizmo_operation = ImGuizmo::SCALE;

    if (ImGui::RadioButton("Translate", editor->guizmo_operation == ImGuizmo::TRANSLATE)) {
        editor->guizmo_operation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", editor->guizmo_operation == ImGuizmo::ROTATE)) {
        editor->guizmo_operation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", editor->guizmo_operation == ImGuizmo::SCALE)) {
        editor->guizmo_operation = ImGuizmo::SCALE;
    }

    if (editor->guizmo_operation != ImGuizmo::SCALE) {
        if (ImGui::RadioButton("Local", editor->guizmo_mode == ImGuizmo::LOCAL)) {
            editor->guizmo_mode = ImGuizmo::LOCAL;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("World", editor->guizmo_mode == ImGuizmo::WORLD)) {
            editor->guizmo_mode = ImGuizmo::WORLD;
        }
    } else {
        editor->guizmo_mode = ImGuizmo::LOCAL;
    }

    glm::vec3 position, rotation, scale;
    glm::mat4 matrix = editor->guizmo_mode == ImGuizmo::LOCAL ? local_matrix : world_matrix;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix),
                                          glm::value_ptr(position),
                                          glm::value_ptr(rotation),
                                          glm::value_ptr(scale));
    glm::vec3 old_position = position;
    glm::vec3 old_rotation = rotation;
    glm::vec3 old_scale    = scale;
    bool is_dirty = false;
    is_dirty |= ImGui::InputFloat3("Position", glm::value_ptr(position), 3);
    is_dirty |= ImGui::InputFloat3("Rotation", glm::value_ptr(rotation), 3);
    is_dirty |= ImGui::InputFloat3("Scale",    glm::value_ptr(scale),    3);

    if (is_dirty) {
        ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(position),
                                                glm::value_ptr(rotation),
                                                glm::value_ptr(scale),
                                                glm::value_ptr(matrix));

        if (editor->guizmo_mode == ImGuizmo::LOCAL) {
            if (pos) pos->v = position;
            if (euler_rot) euler_rot->v = rotation;
            if (scl) scl->v = scale;

            if (local_to_parent) {
                local_to_parent->m = matrix;
            } else {
                local_to_world->m = matrix;
            }
        } else if (editor->guizmo_mode == ImGuizmo::WORLD) {
            glm::vec3 local_position, local_rotation, local_scale;
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(local_matrix),
                                                  glm::value_ptr(local_position),
                                                  glm::value_ptr(local_rotation),
                                                  glm::value_ptr(local_scale));
            local_position += position - old_position;
            local_rotation += rotation - old_rotation;
            local_scale    += scale    - old_scale;
            ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(local_position),
                                                    glm::value_ptr(local_rotation),
                                                    glm::value_ptr(local_scale),
                                                    glm::value_ptr(local_matrix));

            if (pos) pos->v = local_position;
            if (euler_rot) euler_rot->v = local_rotation;
            if (scl) scl->v = local_scale;

            if (local_to_parent) {
                local_to_parent->m = local_matrix;
            } else {
                local_to_world->m = matrix;
            }
        }
    }

    matrix = world_matrix; // NOTE(alexander): guizmo only works in world space
    glm::mat4 delta_matrix;
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    is_dirty = ImGuizmo::Manipulate(glm::value_ptr(camera->view),
                                    glm::value_ptr(camera->proj),
                                    editor->guizmo_operation,
                                    editor->guizmo_mode,
                                    glm::value_ptr(matrix),
                                    glm::value_ptr(delta_matrix));

    if (is_dirty) {
        if (local_to_parent) {
            // NOTE(alexander): conversion from world space to local space.
            glm::mat4 inv_parent_world;
            auto parent = (Parent*) _get_component(world, entity, Parent_ID, Parent_SIZE);
            if (parent) {
                auto parent_local_to_world = get_component(world, parent->handle, Local_To_World);
                if (parent_local_to_world) {
                    inv_parent_world = glm::inverse(parent_local_to_world->m);
                }
            }
            
            // Changes the local or world matrix 
            glm::mat4 new_local_matrix = inv_parent_world * matrix;
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(new_local_matrix),
                                                  glm::value_ptr(position),
                                                  glm::value_ptr(rotation),
                                                  glm::value_ptr(scale));
            
            
            if (pos) pos->v = position;
            if (euler_rot) euler_rot->v = rotation;
            if (scl) scl->v = scale;
            local_to_parent->m = new_local_matrix;
        } else {
            // Changes always the world matrix 
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix),
                                                  glm::value_ptr(position),
                                                  glm::value_ptr(rotation),
                                                  glm::value_ptr(scale));

            if (pos) pos->v = position;
            if (euler_rot) euler_rot->v = rotation;
            if (scl) scl->v = scale;
            local_to_world->m = matrix;
        }
    }
}

void
build_entity_hierarchy(World_Editor* editor, World* world, Entity* entity) {
    auto child = (Child*) _get_component(world, entity, Child_ID, Child_SIZE);
    auto debug_name = (Debug_Name*) _get_component(world, entity, Debug_Name_ID, Debug_Name_SIZE);
    auto entity_name = debug_name ? debug_name->s.c_str() : default_entity_name;
    auto node_flags = child ? ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
        : ImGuiTreeNodeFlags_Leaf;
    
    bool is_open = ImGui::TreeNodeEx((void*) entity->handle.id, node_flags, entity_name);
    if (ImGui::IsItemClicked() &&
        (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x) > ImGui::GetTreeNodeToLabelSpacing()) {
        editor->selected = entity->handle;
    }

    if (is_open) {
        for (auto component : entity->components) {
            if (component.id == Child_ID) {
                std::vector<u8>& memory = world->components[Child_ID];
                child = (Child*) &memory[component.offset];
                Entity* child_entity = get_entity(world, child->handle);
                build_entity_hierarchy(editor, world, child_entity);
            }
        }
        ImGui::TreePop();
    }
}

void
render_world_editor(World_Editor* editor, Window* window, f32 dt) {
    World* world = editor->world;

    // Render the world
    auto camera = get_component(world, editor->editor_camera, Camera);
    begin_frame(world->renderer.fog_color, camera->viewport, true, &world->renderer);
    update_systems(world, editor->rendering_pipeline, dt);
    end_frame();

    ImGui::Begin("Hierarchy", &editor->show_hierarchy);
    for (u32 i = 0; i < world->entities.size(); i++) {
        Entity* entity = &world->entities[i];
        if (is_alive(world, entity->handle)) {
            if (_get_component(world, entity, Parent_ID, Parent_SIZE)) {
                continue; // Gets added by its parent instead
            }

            build_entity_hierarchy(editor, world, entity);
        }
    }
    ImGui::End();

    ImGui::Begin("Inspector", &editor->show_inspector);
    Entity* entity = get_entity(world, editor->selected);
    if (entity) {
        auto debug_name = (Debug_Name*) _get_component(world, entity, Debug_Name_ID, Debug_Name_SIZE);
        ImGui::Text(debug_name ? debug_name->s.c_str() : default_entity_name);
        edit_transform(editor, world, camera, entity);
    }
    ImGui::End();
}
