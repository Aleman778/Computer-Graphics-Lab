
/***************************************************************************
 * Entity management
 ***************************************************************************/

static constexpr usize default_component_capacity = 2; // TODO(alexander): tweak this

static constexpr usize min_removed_indices = 1024;

static constexpr usize entity_index_bits = 24;
static constexpr usize entity_index_mask = (1<<entity_index_bits) - 1;

static constexpr usize entity_generation_bits = 8;
static constexpr usize entity_generation_mask = (1<<entity_generation_bits) - 1;

static inline u32
get_entity_index(Entity_Handle entity) {
    return entity.id & entity_index_mask;
}

static inline u8
get_entity_generation(Entity_Handle entity) {
    return (u8) ((entity.id >> entity_index_bits) & entity_generation_mask);
}

static inline Entity_Handle
make_entity_handle(u32 index, u8 generation) {
    Entity_Handle entity = { index | (generation << entity_index_bits) };
    return entity;
}

inline Entity*
get_entity(World* world, Entity_Handle handle) {
    int i = world->handles[get_entity_index(handle)];
    return &world->entities[i];
}

inline bool
is_alive(World* world, Entity_Handle entity) {
    return world->generations[get_entity_index(entity)] == get_entity_generation(entity);
}

Entity_Handle
spawn_entity(World* world) {
    u32 index;
    if (world->removed_entity_indices.size() > min_removed_indices) {
        index = world->removed_entity_indices[0];
        world->removed_entity_indices.pop_front();
        world->handles[index] = (u32) world->entities.size();
    } else {
        index = (u32) world->generations.size();
        assert(index < (1 << entity_index_bits));
        world->generations.push_back(0);
        world->handles.push_back((u32) world->entities.size());
    }

    Entity entity = {};
    Entity_Handle handle = make_entity_handle(index, world->generations[index]);
    entity.handle = handle;
    world->entities.push_back(entity);
    return handle;
}

void
despawn_entity(World* world, Entity_Handle entity) {
    u32 index = get_entity_index(entity);
    world->generations[index]++;
    world->removed_entity_indices.push_back(index);

    u32 entity_index = world->handles[index];
    world->entities[entity_index] = world->entities[world->entities.size() - 1];
    world->handles[index] = entity_index;
    world->entities.pop_back();
    world->handles.pop_back();
}

Entity_Handle
copy_entity(World* world, Entity_Handle entity) {
    assert(is_alive(world, entity));
    Entity_Handle copy = spawn_entity(world); // TODO(alexander): manage new entity manager later!
    return copy;
}

/***************************************************************************
 * Component management
 ***************************************************************************/

#define add_component(world, handle, type) \
    (type*) _add_component(world, get_entity(world, handle), type ## _ID, type ## _SIZE)
#define remove_component(world, handle, type) \
    _remove_component(world, get_entity(world, handle), type ## _ID, type ## _SIZE)
#define get_component(world, handle, type) \
    (type*) _get_component(world, get_entity(world, handle), type ## _ID, type ## _SIZE)

void*
_add_component(World* world, Entity* entity, u32 id, usize size) {
    assert(is_alive(world, entity->handle) && "cannot add component to despawned entity");

    if (world->components.find(id) == world->components.end()) {
        world->components[id].reserve(size*default_component_capacity);
    }

    std::vector<u8>& memory = world->components[id];
    usize curr_size = memory.size();
    memory.resize(curr_size + size); // TODO(alexander): use custom allocator

    // NOTE(alexander): each component data needs to keep track of its owner handle
    *((Entity_Handle*) &memory[curr_size]) = entity->handle;
    curr_size += sizeof(Entity_Handle);

    Component_Handle component = { (u32) id, curr_size };
    entity->components.push_back(component);
    return &memory[curr_size];
}

bool
_remove_component(World* world, Entity* entity, u32 id, usize size) {
    assert(is_alive(world, entity->handle) && "cannot remove component from despawned entity");

    for (int i = 0; i < entity->components.size(); i++) {
        Component_Handle& component = entity->components[i];

        if (component.id == id) {
            std::vector<u8>& memory = world->components[id]; // TODO(alexander): use custom allocator
            usize index = component.offset - sizeof(Entity_Handle);
            usize last_index = memory.size() - size - sizeof(Entity_Handle);
            Entity_Handle* last = (Entity_Handle*) &memory[last_index];

            if (index != last_index) {
                Entity* last_entity = get_entity(world, *last);
                for (auto last_component : last_entity->components) {
                    if (last_component.id == component.id && last_component.offset == last_index) {
                        last_component.offset = component.offset;
                        break;
                    }
                }

                memcpy(&memory[component.id], last, size);
            }

            memory.resize(last_index);

            entity->components[i] = entity->components[entity->components.size() - 1];
            entity->components.pop_back();
        }
    }

    return false;
}

void*
_get_component(World* world, Entity* entity, u32 id, usize size) {
    assert(is_alive(world, entity->handle) && "cannot get component from despawned entity");
    
    for (int i = 0; i < entity->components.size(); i++) {
        Component_Handle& component = entity->components[i];

        if (component.id == id) {
            std::vector<u8>& memory = world->components[id];
            return &memory[component.offset];
        }
    }

    return NULL;
}

// TODO(alexander): maybe add get_all_components or more than one component
// TODO(alexander): should it be possible to have two of the same component on one entity?

/***************************************************************************
 * Systems management
 ***************************************************************************/

#define use_component(system, type, ...) \
    _use_component(system, type ## _ID, type ## _SIZE, __VA_ARGS__)

void
_use_component(System* system, u32 id, u32 size, u32 flags=0) {
    system->component_ids.push_back(id);
    system->component_sizes.push_back(size);
    system->component_flags.push_back(flags);
}

void
register_system(World* world, System system) {
    assert(system.component_ids.size() > 0 && "expected system to use at least one component");
    bool is_valid = false;
    for (int i = 0; i < system.component_flags.size(); i++) {
        if ((system.component_flags[i] & System::Flag_Optional) == 0) {
            is_valid = true;
            break;
        }
    }
    assert(is_valid && "invalid system, all components cannot be optional");
    world->systems.push_back(system);
}

void
update_systems(World* world, f32 dt) {
    std::vector<void*> component_params;
    std::vector<std::vector<u8>*> component_data;

    for (u32 i = 0; i < world->systems.size(); i++) {
        System* system = &world->systems[i];

        if (system->component_ids.size() == 1) {
            u32 id = system->component_ids[0];
            u32 size = system->component_sizes[0];
            std::vector<u8>& data = world->components[id];
            for (int j = 0; j < data.size(); j += size) {
                Entity_Handle handle = *((Entity_Handle*) &data[j]);
                void* component = &data[j + sizeof(Entity_Handle)];
                system->on_update(world, dt, handle, &component);
            }

        } else {
            component_params.resize(max(component_params.size(), system->component_ids.size()));
            component_data.resize(max(component_data.size(), system->component_ids.size()));

            u32 min_size = (u32) -1;
            u32 min_index = (u32) -1;
            for (int j = 0; j < system->component_ids.size(); j++) {
                component_data[j] = &world->components[system->component_ids[j]];
                if ((system->component_flags[j] & System::Flag_Optional) != 0) {
                    continue;
                }

                u32 id = system->component_ids[0];
                u32 size = system->component_sizes[0];
                if (size <= min_size) {
                    min_size = size;
                    min_index = i;
                }
            }
            assert(min_index < component_params.size());

            std::vector<u8>& data = *component_data[min_index];
            for (int j = 0; j < data.size(); j += min_size) {
                Entity_Handle handle = *((Entity_Handle*) &data[j]);
                Entity* entity = get_entity(world, handle);
                component_params[min_index] = &data[j + sizeof(Entity_Handle)];
                
                bool is_valid = true;
                for (int k = 0; k < system->component_ids.size(); k++) {
                    if (k == min_index) continue;
                    component_params[k] = _get_component(world,
                                                         entity,
                                                         system->component_ids[k],
                                                         system->component_sizes[k]);
                    if (component_params[k] == NULL && (system->component_flags[k] & System::Flag_Optional) == 0) {
                        is_valid = false;
                        break;
                    }
                }

                if (is_valid) {
                    system->on_update(world, dt, handle, &component_params[0]);
                }
            }
        }
    }
}

/***************************************************************************
 * Basic Non-hierarchical Transform System
 ***************************************************************************/

DEF_ON_UPDATE(convert_euler_rotation_system) {
    auto euler_rot = (Euler_Rotation*) components[0];
    auto rot = (Rotation*) components[1];
    
    rot->q = glm::quat(euler_rot->v);
}

DEF_ON_UPDATE(trs_local_to_world_system) {
    auto local_to_world = (Local_To_World*) components[0];
    auto pos = (Position*) components[1];
    auto rot = (Rotation*) components[2];
    auto scl = (Scale*) components[3];

    glm::mat4 T = pos ? glm::translate(glm::mat4(1.0f), pos->v) : glm::mat4(1.0f);
    glm::mat4 R = rot ? glm::toMat4(rot->q)                     : glm::mat4(1.0f);
    glm::mat4 S = scl ? glm::scale(glm::mat4(1.0f), scl->v)     : glm::mat4(1.0f);

    local_to_world->m = T * R * S;
}

void
register_transform_systems(World* world) {
    System euler_conv = {};
    euler_conv.on_update = &convert_euler_rotation_system;
    use_component(&euler_conv, Euler_Rotation);
    use_component(&euler_conv, Rotation);
    register_system(world, euler_conv);
    
    System trs_world = {};
    trs_world.on_update = &trs_local_to_world_system;
    use_component(&trs_world, Local_To_World);
    use_component(&trs_world, Position, System::Flag_Optional);
    use_component(&trs_world, Rotation, System::Flag_Optional);
    use_component(&trs_world, Scale,    System::Flag_Optional);
    register_system(world, trs_world);
}

/***************************************************************************
 * Camera systems
 ***************************************************************************/

// NOTE(alexander): same as trs_local_to_world_system but inverted and without scale
DEF_ON_UPDATE(tr_view_matrix_system) {
    auto camera = (Camera*) components[0];
    auto pos = (Position*) components[1];
    auto rot = (Rotation*) components[2];

    glm::mat4 T = pos ? glm::translate(glm::mat4(1.0f), -pos->v) : glm::mat4(1.0f);
    glm::mat4 R = rot ? glm::toMat4(rot->q)                      : glm::mat4(1.0f);

    camera->view = T * R;
}

DEF_ON_UPDATE(set_projection_system) {
    auto camera = (Camera*) components[0];

    if (camera->is_orthographic) {
        if (camera->near == 0 && camera->far == 0) {
            camera->proj = glm::ortho(camera->left,
                                      camera->right,
                                      camera->bottom,
                                      camera->top);
        } else {
            camera->proj = glm::ortho(camera->left,
                                      camera->right,
                                      camera->bottom,
                                      camera->top,
                                      camera->near,
                                      camera->far);
        }
    } else {
        camera->proj = glm::perspective(camera->fov,
                                        camera->aspect,
                                        camera->near,
                                        camera->far);
    }
}

void
register_camera_systems(World* world) {
    System view_system = {};
    view_system.on_update = &tr_view_matrix_system;
    use_component(&view_system, Camera);
    use_component(&view_system, Position, System::Flag_Optional);
    use_component(&view_system, Rotation, System::Flag_Optional);
    register_system(world, view_system);
    
    System projection_system = {};
    projection_system.on_update = &set_projection_system;
    use_component(&projection_system, Camera);
    register_system(world, projection_system);
}


/***************************************************************************
 * Rendering World
 ***************************************************************************/

void
render_world(World* world) {
    if (!is_alive(world, world->main_camera)) return;

    // Camera
    auto camera = get_component(world, world->main_camera, Camera);
    auto camera_pos = get_component(world, world->main_camera, Position);
    assert(camera && "missing required Camera component on main camera");
    assert(camera_pos && "missing required Position component on main camera");
    world->light_setup.view_position = camera_pos->v;
    glm::mat4 view_proj_matrix = camera->proj * camera->view;

    begin_frame(world->clear_color, camera->viewport, world->depth_testing);

    Material_Type prev_material = Material_Type_None;
    std::vector<u8>& data = world->components[Mesh_Renderer_ID];
    for (u32 i = 0; i < data.size(); i += Mesh_Renderer_SIZE) {
        Entity* entity = get_entity(world, *((Entity_Handle*) &data[i]));
        if (!is_alive(world, entity->handle)) continue;

        auto local_to_world = (Local_To_World*) _get_component(world, entity, Local_To_World_ID, Local_To_World_SIZE);
        glm::mat4 model_matrix = local_to_world ? local_to_world->m : glm::mat4(1.0f);

        auto mesh_renderer = (Mesh_Renderer*) &data[i + sizeof(Entity_Handle)];
        const Mesh mesh = mesh_renderer->mesh;
        const Material material = mesh_renderer->material;

        if (material.type != prev_material) {
            apply_material(material,
                           &world->light_setup,
                           world->clear_color,
                           world->fog_density,
                           world->fog_gradient);
        }

        draw_mesh(mesh, material, model_matrix, camera->view, camera->proj, view_proj_matrix);
    }

    end_frame();
}
