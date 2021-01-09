
/**
 * Entities handles are represented by an identifier that contains both index and generation.
 *  31        24 23                                           0
 * +------------+----------------------------------------------+
 * | generation | index                                        |
 * +------------+----------------------------------------------+
 *
 * Entities are stored in a contiguous array indexed by the first 24 bits
 * of the entity handle id. The upper 8 bits are used for generation which differentiate
 * between nodes that reuse the same (previously deleted nodes) index.
 */
struct Entity_Handle {
    u32 id;
};

inline bool
operator==(const Entity_Handle& a, const Entity_Handle& b) {
    return a.id == b.id;
}

namespace std {
    template <>
    struct hash<Entity_Handle> {
        std::size_t operator()(const Entity_Handle& k) const {
            return std::hash<u32>()(k.id);
        }
    };
}

static u32 next_component_id = 0;

#define REGISTER_COMPONENT(type) \
    static const u32 type ## _ID = next_component_id++;    \
    static const u32 type ## _SIZE = sizeof(type) + sizeof(Entity_Handle)

/***************************************************************************
 * Common Components
 ***************************************************************************/

struct Local_To_World {
    glm::mat4 m;
};

struct Position {
    glm::vec3 v;
};

struct Euler_Rotation {
    glm::vec3 v;
};

struct Rotation {
    glm::quat q;
};

struct Scale {
    glm::vec3 v;
};

struct Camera {
    f32 fov;
    f32 left;
    f32 right;
    f32 bottom;
    f32 top;
    f32 near;
    f32 far;
    f32 aspect;
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 view_proj;
    glm::vec4 viewport;
    bool is_orthographic; // perspective (false), orthographic (true)
};

struct Mesh_Renderer {
    Mesh mesh;
    Material material;
};

struct Point_Light {
    glm::vec3 color;
    f32 intensity;
    f32 distance;
};

REGISTER_COMPONENT(Local_To_World);
REGISTER_COMPONENT(Position);
REGISTER_COMPONENT(Euler_Rotation);
REGISTER_COMPONENT(Rotation);
REGISTER_COMPONENT(Scale);
REGISTER_COMPONENT(Camera);
REGISTER_COMPONENT(Mesh_Renderer);
REGISTER_COMPONENT(Point_Light);

/**
 * Handle to a particular instance of a component.
 */
struct Component_Handle {
    u32 id;
    usize offset; // in bytes
};

/**
 * An entity is just an array of components that is identified by its handle.
 */
struct Entity {
    Entity_Handle handle;
    std::vector<Component_Handle> components;
};

struct World;

/**
 * On update system function pointer type takes the
 * delta time, entity handle and list of component data as input.
 */
typedef void (*OnUpdateSystem)(World* world, f32 dt, Entity_Handle entity, void** components, void* data);

#define DEF_SYSTEM(system_name) \
    void system_name(World* world, f32 dt, Entity_Handle handle, void** components, void* data)

/**
 * System is just a function that takes some number of components as input.
 * This is a helper structure that defines the function pointer to call and
 * its components to take in as argument (i.e. void** components argument).
 */
struct System {
    enum {
        Flag_Optional = 1,
    };

    void* data;
    OnUpdateSystem on_update;
    std::vector<u32> component_ids;
    std::vector<u32> component_sizes;
    std::vector<u32> component_flags;
};

/**
 * World is where everyting in the entity component system is defined.
 * Entities are managed here, component data is stored here and systems are defined here.
 */
struct World {
    std::vector<u8> generations;
    std::deque<u32> removed_entity_indices;

    std::vector<Entity> entities; // all the live entities
    std::vector<u32> handles; // lookup entity by handle, NOTE: only valid if the entity itself is alive!!!
    std::unordered_map<u32, std::vector<u8>> components; // where component data is stored

    // should not be stored here!
    Light_Setup light;
    f32 fog_density;
    f32 fog_gradient;
    glm::vec4 clear_color;
};

inline bool is_alive(World* world, Entity_Handle entity);
Entity_Handle spawn_entity(World* world);
void despawn_entity(World* world, Entity_Handle entity);
Entity_Handle copy_entity(World* world, Entity_Handle entity);

void* _add_component(World* world, Entity_Handle handle, u32 id, usize size);
bool _remove_component(World* world, Entity_Handle handle, u32 id, usize size);
void* _get_component(World* world, Entity_Handle handle, u32 id, usize size);
void _use_component(System& system, u32 id, u32 size, u32 flags=0);
void push_system(std::vector<System>& systems, System system);
void update_systems(const std::vector<System>& systems, f32 dt);
