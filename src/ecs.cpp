
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

/***************************************************************************
 * Systems management
 ***************************************************************************/

#define use_component(system, type, ...) \
    _use_component(system, type ## _ID, type ## _SIZE, __VA_ARGS__)

void
_use_component(System& system, u32 id, u32 size, u32 flags) {
    system.component_ids.push_back(id);
    system.component_sizes.push_back(size);
    system.component_flags.push_back(flags);
}

void
push_system(std::vector<System>& systems, System system) {
    assert(system.component_ids.size() > 0 && "expected system to use at least one component");
    bool is_valid = false;
    for (int i = 0; i < system.component_flags.size(); i++) {
        if ((system.component_flags[i] & System::Flag_Optional) == 0) {
            is_valid = true;
            break;
        }
    }
    assert(is_valid && "invalid system, all components cannot be optional");
    systems.push_back(system);
}

void
update_systems(World* world, const std::vector<System>& systems, f32 dt) {
    std::vector<void*> component_params;
    std::vector<std::vector<u8>*> component_data;

    for (u32 i = 0; i < systems.size(); i++) {
        const System& system = systems[i];
        assert(system.on_update && "system is missing on_update function");

        if (system.component_ids.size() == 1) {
            u32 id = system.component_ids[0];
            u32 size = system.component_sizes[0];
            std::vector<u8>& data = world->components[id];
            for (int j = 0; j < data.size(); j += size) {
                Entity_Handle handle = *((Entity_Handle*) &data[j]);
                void* component = &data[j + sizeof(Entity_Handle)];
                system.on_update(world, dt, handle, &component, system.data);
            }

        } else {
            component_params.resize(max(component_params.size(), system.component_ids.size()));
            component_data.resize(max(component_data.size(), system.component_ids.size()));

            u32 min_size = (u32) -1;
            u32 min_index = (u32) -1;
            for (int j = 0; j < system.component_ids.size(); j++) {
                component_data[j] = &world->components[system.component_ids[j]];
                if ((system.component_flags[j] & System::Flag_Optional) != 0) {
                    continue;
                }

                u32 id = system.component_ids[j];
                u32 size = system.component_sizes[j];
                if (size <= min_size) {
                    min_size = size;
                    min_index = j;
                }
            }
            assert(min_index < component_params.size());

            std::vector<u8>& data = *component_data[min_index];
            for (int j = 0; j < data.size(); j += min_size) {
                Entity_Handle handle = *((Entity_Handle*) &data[j]);
                Entity* entity = get_entity(world, handle);
                component_params[min_index] = &data[j + sizeof(Entity_Handle)];
                
                bool is_valid = true;
                for (int k = 0; k < system.component_ids.size(); k++) {
                    if (k == min_index) continue;
                    component_params[k] = _get_component(world,
                                                         entity,
                                                         system.component_ids[k],
                                                         system.component_sizes[k]);
                    if (component_params[k] == NULL && (system.component_flags[k] & System::Flag_Optional) == 0) {
                        is_valid = false;
                        break;
                    }
                }

                if (is_valid) {
                    system.on_update(world, dt, handle, &component_params[0], system.data);
                }
            }
        }
    }
}

/***************************************************************************
 * Basic Non-hierarchical Transform System
 ***************************************************************************/

DEF_SYSTEM(convert_euler_rotation_system) {
    auto euler_rot = (Euler_Rotation*) components[0];
    auto rot = (Rotation*) components[1];

    // NOTE(alexander): y - points upwards, but glm uses z instead
    glm::quat rot_x(glm::vec3(0.0f, euler_rot->v.x, 0.0f));
    glm::quat rot_y(glm::vec3(euler_rot->v.y, 0.0f, 0.0f));
    glm::quat rot_z(glm::vec3(0.0f, 0.0f, euler_rot->v.z));
    rot->q = rot_z * rot_y * rot_x;
}

DEF_SYSTEM(trs_local_to_world_system) {
    auto local_to_world = (Local_To_World*) components[0];
    auto pos = (Position*) components[1];
    auto rot = (Rotation*) components[2];
    auto scl = (Scale*) components[3];

    if (!pos && !rot && !scl) return; // NOTE(alexander): need at least one of these

    glm::mat4 T = pos ? glm::translate(glm::mat4(1.0f), pos->v) : glm::mat4(1.0f);
    glm::mat4 R = rot ? glm::toMat4(rot->q)                     : glm::mat4(1.0f);
    glm::mat4 S = scl ? glm::scale(glm::mat4(1.0f), scl->v)     : glm::mat4(1.0f);

    local_to_world->m = T * R * S;
}

void
push_transform_systems(std::vector<System>& systems) {
    System euler_conv = {};
    euler_conv.on_update = &convert_euler_rotation_system;
    use_component(euler_conv, Euler_Rotation);
    use_component(euler_conv, Rotation);
    push_system(systems, euler_conv);
    
    System trs_world = {};
    trs_world.on_update = &trs_local_to_world_system;
    use_component(trs_world, Local_To_World);
    use_component(trs_world, Position, System::Flag_Optional);
    use_component(trs_world, Rotation, System::Flag_Optional);
    use_component(trs_world, Scale,    System::Flag_Optional);
    push_system(systems, trs_world);
}

/***************************************************************************
 * Camera systems
 ***************************************************************************/

// NOTE(alexander): same as trs_local_to_world_system but inverted and without scale
DEF_SYSTEM(rt_view_matrix_system) {
    auto camera = (Camera*) components[0];
    auto pos = (Position*) components[1];
    auto rot = (Rotation*) components[2];

    glm::mat4 T = pos ? glm::translate(glm::mat4(1.0f), -pos->v) : glm::mat4(1.0f);
    glm::mat4 R = rot ? glm::toMat4(rot->q)                      : glm::mat4(1.0f);

    camera->view = R * T;
}

DEF_SYSTEM(set_projection_system) {
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

DEF_SYSTEM(prepare_camera_system) {
    auto camera = (Camera*) components[0];
    auto camera_pos = (Position*) components[1];
    if (camera_pos) world->light.view_pos = camera_pos->v;
    camera->view_proj = camera->proj * camera->view;
}

void
push_camera_systems(std::vector<System>& systems) {
    System view_system = {};
    view_system.on_update = &rt_view_matrix_system;
    use_component(view_system, Camera);
    use_component(view_system, Position, System::Flag_Optional);
    use_component(view_system, Rotation, System::Flag_Optional);
    push_system(systems, view_system);
    
    System projection_system = {};
    projection_system.on_update = &set_projection_system;
    use_component(projection_system, Camera);
    push_system(systems, projection_system);

    System prepare_system = {};
    prepare_system.on_update = &prepare_camera_system;
    use_component(prepare_system, Camera);
    use_component(prepare_system, Position, System::Flag_Optional);
    push_system(systems, prepare_system);
}


/***************************************************************************
 * Rendering Systems
 ***************************************************************************/

DEF_SYSTEM(mesh_renderer_system) {
    assert(data && "missing targeted camera for rendering to");
    
    auto camera = get_component(world, *((Entity_Handle*) data), Camera);
    auto mesh_renderer = (Mesh_Renderer*) components[0];
    auto local_to_world = (Local_To_World*) components[1];

    assert(camera && "missing camera component on target camera entity");
    
    glm::mat4 model_matrix = local_to_world ? local_to_world->m : glm::mat4(1.0f);
    const Mesh mesh = mesh_renderer->mesh;
    const Material material = mesh_renderer->material;

    apply_material(material,
                   &world->light,
                   world->clear_color,
                   world->fog_density,
                   world->fog_gradient);
    draw_mesh(mesh, material, model_matrix, camera->view, camera->proj, camera->view_proj);
}

void
push_mesh_renderer_system(std::vector<System>& systems, Entity_Handle* camera) {
    System system = {};
    system.data = camera;
    system.on_update = &mesh_renderer_system;
    use_component(system, Mesh_Renderer);
    use_component(system, Local_To_World, System::Flag_Optional);
    push_system(systems, system);
}
