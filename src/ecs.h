
/**
 * Entities are represented by an identifier that contains both index and generation.
 *  31        24 23                                           0
 * +------------+----------------------------------------------+
 * | generation | index                                        |
 * +------------+----------------------------------------------+
 *
 * Entities are stored in a contiguous array indexed by the first 24 bits
 * of the entity id. The upper 8 bits are used for generation which differentiate
 * between nodes that reuse the same (previously deleted nodes) index.
 */
struct Entity {
    u32 id;
};

inline bool
operator==(const Entity& a, const Entity& b) {
    return a.id == b.id;
}

namespace std {
    template <>
    struct hash<Entity> {
        std::size_t operator()(const Entity& k) const {
            return std::hash<u32>()(k.id);
        }
    };
}

/**
 * SoA packed array of transform data.
 */
struct Transform_Data {
    u8* buffer;
    u32 count;
    u32 capacity;

    Entity* entity;
    glm::mat4* local;
    glm::mat4* world;
    u32* parent; // index into this lists
    u32* first_child;
    u32* prev_sibling;
    u32* next_sibling;
};

/**
 * Manages all the registered transform components.
 */
struct Transforms {
    std::unordered_map<Entity, u32> map;
    Transform_Data data;
};

/**
 * SoA packed array of mesh renderer data.
 */
struct Mesh_Data {
    u8* buffer;
    u32 count;
    u32 capacity;

    Entity* entity;
    Mesh* mesh;
    Material* material;
};

/**
 * Manages all the registered mesh renderer components.
 */
struct Mesh_Renderers {
    std::unordered_map<Entity, u32> map;
    Mesh_Data data;
};

/**
 * Stored as AoS, there are not going to be many of these active
 * so the memory layout does not matter as much here.
 */
struct Camera_Data {
    Entity entity;
    f32 fov;
    f32 left;
    f32 right;
    f32 bottom;
    f32 top;
    f32 near;
    f32 far;
    f32 aspect_ratio;
    glm::vec3 view_position;
    glm::mat4 projection_matrix; // projection * view (aka. camera transform)
    glm::vec4 viewport;
    bool is_orthographic; // perspective (false), orthographic (true)
};

/**
 * Manages all the registered camera components.
 */
struct Cameras {
    std::unordered_map<Entity, Camera_Data> map;
};

/**
 * World is where all entities live.
 */
struct World {
    std::vector<u8> generations;
    std::deque<u32> removed_entity_indices;

    Transforms transforms;
    Mesh_Renderers mesh_renderers;
    Cameras cameras;

    Entity main_camera;

    Light_Setup light_setup;
    f32 fog_density;
    f32 fog_gradient;
    glm::vec4 clear_color;
    bool depth_testing;
};

inline u32 get_entity_index(Entity entity);
inline u8 get_entitiy_generation(Entity entity);
inline Entity make_entity(u32 index, u8 generation);

Entity spawn_entity(World* world);
void despawn_entity(World* world, Entity entity);

void set_local_transform(World* world, Entity entity, const glm::mat4& m);
void destroy_transform(World* world, Entity entity);
void set_parent(World* world, Entity entity, Entity parent_entity);

void set_mesh(World* world, Entity entity, Mesh mesh, Material material);

void set_perspective(World* world,
                     Entity entity,
                     f32 fov=glm::radians(90.0f),
                     f32 near=0.1f,
                     f32 far=100000.0f,
                     f32 aspect_ratio=1.0f,
                     glm::vec4 viewport=glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

void set_orthographic(World* world,
                      Entity entity,
                      f32 left,
                      f32 right,
                      f32 top,
                      f32 bottom,
                      f32 near=0,
                      f32 far=0,
                      glm::vec4 viewport=glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    
void render_world(World* world);
