
/***************************************************************************
 * Entity management
 ***************************************************************************/

static constexpr usize default_min_entities = 100;

static constexpr usize min_removed_indices = 1024;

static constexpr usize entity_index_bits = 24;
static constexpr usize entity_index_mask = (1<<entity_index_bits) - 1;

static constexpr usize entity_generation_bits = 8;
static constexpr usize entity_generation_mask = (1<<entity_generation_bits) - 1;

inline u32
get_entity_index(Entity entity) {
    return entity.id & entity_index_mask;
}

inline u8
get_entity_generation(Entity entity) {
    return (u8) ((entity.id >> entity_index_bits) & entity_generation_mask);
}

inline Entity
make_entity(u32 index, u8 generation) {
    Entity entity = { index | (generation << entity_index_bits) };
    return entity;
}

Entity
spawn_entity(World* world) {
    u32 index;
    if (world->removed_entity_indices.size() > min_removed_indices) {
        index = world->removed_entity_indices[0];
        world->removed_entity_indices.pop_front();
    } else {
        index = world->generations.size();
        assert(index < (1 << entity_index_bits));
        world->generations.push_back(0);
    }
    return make_entity(index, world->generations[index]);
}

void
despawn_entity(World* world, Entity entity) {
    u32 index = get_entity_index(entity);
    world->generations[index]++;
    world->removed_entity_indices.push_back(index);
    destroy_transform(world, entity);
}

inline bool
is_alive(World* world, Entity entity) {
    world->generations[get_entity_index(entity)] == get_entity_generation(entity);
}

/***************************************************************************
 * Transform Component
 ***************************************************************************/

static void
ensure_transform_capacity(Transform_Data* data, usize new_capacity) {
    if (new_capacity > data->capacity) {
        Transform_Data new_data;
        new_data.capacity = data->capacity;
        new_data.count = data->count;

        if (new_data.capacity == 0) {
            new_data.capacity = 100;
        } else {
            new_data.capacity *= 2;
        }
        if (new_capacity > new_data.capacity) new_data.capacity = new_capacity;

        usize size = new_data.capacity*(sizeof(Entity) + 2*sizeof(glm::mat4));
        new_data.buffer       = (u8*) malloc(size);
        new_data.entity       = (Entity*)     new_data.buffer;
        new_data.local        = (glm::mat4*) (new_data.entity + new_data.capacity);
        new_data.world        = (glm::mat4*) (new_data.local + new_data.capacity);
        new_data.parent       = (u32*)       (new_data.world + new_data.capacity);
        new_data.first_child  = (u32*)       (new_data.parent + new_data.capacity);
        new_data.prev_sibling = (u32*)       (new_data.first_child + new_data.capacity);
        new_data.next_sibling = (u32*)       (new_data.prev_sibling + new_data.capacity);

        if (data->buffer && data->count > 0) {
            memcpy(new_data.entity, data->entity, data->count*sizeof(Entity));
            memcpy(new_data.local, data->local, data->count*sizeof(glm::mat4));
            memcpy(new_data.world, data->world, data->count*sizeof(glm::mat4));
            memcpy(new_data.parent, data->parent, data->count*sizeof(u32));
            memcpy(new_data.first_child, data->first_child, data->count*sizeof(u32));
            memcpy(new_data.prev_sibling, data->prev_sibling, data->count*sizeof(u32));
            memcpy(new_data.next_sibling, data->next_sibling, data->count*sizeof(u32));
            free(data->buffer);
        }

        *data = new_data;
    }
}

static inline u32
lookup_transform(Transforms* transforms, Entity entity) {
    if (transforms->map.find(entity) == transforms->map.end()) {
        return 0;
    }
    return transforms->map[entity];
}

static u32
create_transform(Transforms* transforms, Entity entity) {
    u32 index = transforms->data.count + 1;
    transforms->map[entity] = index;
    ensure_transform_capacity(&transforms->data, index + 1);
    transforms->data.entity[index] = entity;
    transforms->data.local[index] = glm::mat4(1.0f);
    transforms->data.world[index] = glm::mat4(1.0f);
    transforms->data.parent[index] = 0;
    transforms->data.first_child[index] = 0;
    transforms->data.prev_sibling[index] = 0;
    transforms->data.next_sibling[index] = 0;
    transforms->data.count++;
    return index;
}

static void
set_transform(Transform_Data* data, const glm::mat4& parent, u32 index) {
    data->world[index] = data->local[index] * parent;

    u32 child = data->first_child[index];
    while (child) {
        set_transform(data, data->world[index], child); // TODO(alexander): optimization, make iterative
        child = data->next_sibling[child];
    }
}

void
set_local_transform(World* world, Entity entity, const glm::mat4& m) {
    Transform_Data* data = &world->transforms.data;
    u32 index = lookup_transform(&world->transforms, entity);
    if (index) {
        index = create_transform(&world->transforms, entity);
    }

    data->local[index] = m;
    u32 parent = data->parent[index];
    glm::mat4 parent_m = parent ? data->world[parent] : glm::mat4(1.0f);
    set_transform(data, parent_m, parent);
}

void
set_parent(World* world, Entity entity, Entity parent_entity) {
    Transform_Data* data = &world->transforms.data;
    u32 child = lookup_transform(&world->transforms, entity);
    u32 parent = lookup_transform(&world->transforms, entity);
    if (child) child = create_transform(&world->transforms, entity);
    if (parent) parent = create_transform(&world->transforms, parent_entity);

    // Remove previous parent if entity already has a valid parent
    if (data->parent[child]) {
        u32 i = data->parent[child];
        if (data->first_child[i] == child) {
            data->first_child[i] = data->next_sibling[child];
        } else {
            // Follow pointers until this child is found
            i = data->first_child[i];
            while (i != child) {
                i = data->next_sibling[i];
            }

            // Update subling linked list pointers, if found
            assert(i && "unexpected orphan child found");
            u32 prev_sibling = data->prev_sibling[i];
            u32 next_sibling = data->next_sibling[i];
            if (prev_sibling) data->next_sibling[prev_sibling] = next_sibling;
            if (next_sibling) data->prev_sibling[next_sibling] = prev_sibling;
        }
    }

    // Create parent child relationship
    data->parent[child] = parent;
    u32 i = data->first_child[parent];
    if (i) {
        while (data->next_sibling[i] != 0) {
            i = data->next_sibling[i];
        }
        data->next_sibling[i] = child;
    } else {
        data->first_child[parent] = child;
    }

    // Update the childs transform
    set_transform(data, data->world[parent], child);
}

void
destroy_transform(World* world, Entity entity) {
    Transform_Data* data = &world->transforms.data;
    u32 i = lookup_transform(&world->transforms, entity);
    if (i) {
        u32 last = data->count - 1;
        Entity last_entity = data->entity[last];

        // Update subling linked list pointers
        u32 prev_sibling = data->prev_sibling[i];
        u32 next_sibling = data->next_sibling[i];
        if (prev_sibling) data->next_sibling[prev_sibling] = next_sibling;
        if (next_sibling) data->prev_sibling[next_sibling] = prev_sibling;

        data->entity[i]       = data->entity[last];
        data->local[i]        = data->local[last];
        data->world[i]        = data->world[last];
        data->parent[i]       = data->parent[last];
        data->first_child[i]  = data->first_child[last];
        data->prev_sibling[i] = data->prev_sibling[last];
        data->next_sibling[i] = data->next_sibling[last];

        // Update subling linked list pointers for the last entity also
        prev_sibling = data->prev_sibling[i];
        next_sibling = data->next_sibling[i];
        if (prev_sibling) data->next_sibling[prev_sibling] = next_sibling;
        if (next_sibling) data->prev_sibling[next_sibling] = prev_sibling;

        world->transforms.map[last_entity] = i;
        world->transforms.map.erase(entity);
        world->transforms.data.count--;
    }
}

/***************************************************************************
 * Rendering world
 ***************************************************************************/

void
render_world(World* world) {
    if (!world->main_camera) return;

    begin_frame(world->clear_color, world->main_camera->viewport, world->depth_testing);

    glm::mat4 view_proj_matrix = world->main_camera->combined_matrix;
    Material_Type prev_material = Material_Type_None;
    Mesh_Renderer_Data* rendering_data = &world->mesh_renderers.data;
    Transform_Data* transform_data = &world->transforms.data;

    for (u32 i = 0; i < rendering_data->count; i++) {
        Entity entity = rendering_data->entity[i];
        int transform_index = lookup_transform(&world->transforms, entity);
        glm::mat4 model_matrix;
        if (transform_index) {
            model_matrix = transform_data->world[transform_index];
        } else {
            model_matrix = glm::mat4(1.0f);
        }

        Material material = rendering_data->material[i];
        Mesh mesh = rendering_data->mesh[i];

        if (material.type != prev_material) {
            apply_shader(material.shader,
                         &world->light_setup,
                         world->clear_color,
                         world->fog_density,
                         world->fog_gradient);
        }

        draw_mesh(mesh, material, model_matrix, view_proj_matrix);
    }

    end_frame();
}
