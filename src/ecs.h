
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

REGISTER_COMPONENT(Local_To_World);

struct Position {
    glm::vec3 v;
};

REGISTER_COMPONENT(Position);

struct Rotation {
    glm::quat q;
};

REGISTER_COMPONENT(Rotation);

struct Scale {
    glm::vec3 v;
};

REGISTER_COMPONENT(Scale);

struct Mesh_Renderer {
    Mesh mesh;
    Material material;
};

REGISTER_COMPONENT(Mesh_Renderer);

struct View_Projection {
    glm::mat4 view;
    glm::mat4 projection;
};

REGISTER_COMPONENT(View_Projection);

struct Point_Light {
    glm::vec3 color;
    f32 intensity;
    f32 distance;
};

REGISTER_COMPONENT(Point_Light);

/***************************************************************************
 * Transform system
 ***************************************************************************/

struct Debug_Names {
    std::unordered_map<Entity_Handle, std::string> map;
};

/**
 * SoA packed array of transform data.
 */
struct Transform_Data {
    u8* buffer;
    u32 count;
    u32 capacity;

    Entity_Handle* entity;
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
    std::unordered_map<Entity_Handle, u32> map;
    Transform_Data data;
};

/**
 * SoA packed array of mesh renderer data.
 */
struct Mesh_Data {
    u8* buffer;
    u32 count;
    u32 capacity;

    Entity_Handle* entity;
    Mesh* mesh;
    Material* material;
};

/**
 * Manages all the registered mesh renderer components.
 */
struct Mesh_Renderers {
    std::unordered_map<Entity_Handle, u32> map;
    Mesh_Data data;
};

/**
 * Stored as AoS, there are not going to be many of these active
 * so the memory layout does not matter as much here.
 */
struct Camera_Data {
    Entity_Handle entity;
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
    std::unordered_map<Entity_Handle, Camera_Data> map;
};

/**
 * Handle to a particular instance of a component.
 */
struct Component_Handle {
    u32 id;
    u32 offset; // in bytes
};

/**
 * An entity is just an array of components that is identified by its handle.
 */
struct Entity {
    Entity_Handle handle;
    std::vector<Component_Handle> components;
};

/**
 * On update system function pointer type takes the
 * delta time, entity handle and list of component data as input.
 */
typedef void (*OnUpdateSystem)(f32 dt, Entity_Handle entity, void** components);

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
    std::vector<System> systems;

    Debug_Names names;
    Transforms transforms;
    Mesh_Renderers mesh_renderers;
    Cameras cameras;

    Entity_Handle main_camera;

    Light_Setup light_setup;
    f32 fog_density;
    f32 fog_gradient;
    glm::vec4 clear_color;
    bool depth_testing;
};

inline bool is_alive(World* world, Entity_Handle entity);
Entity_Handle spawn_entity(World* world);
void despawn_entity(World* world, Entity_Handle entity);
Entity_Handle copy_entity(World* world, Entity_Handle entity);

void*    _add_component(World* world, Entity_Handle handle, u32 id, usize size);
bool  _remove_component(World* world, Entity_Handle handle, u32 id, usize size);
void*    _get_component(World* world, Entity_Handle handle, u32 id, usize size);

std::string lookup_name(World* world, Entity_Handle entity);
void set_name(World* world, Entity_Handle entity, const char* name);

void set_local_transform(World* world, Entity_Handle entity, const glm::mat4& m);
void destroy_transform(World* world, Entity_Handle entity);
void set_parent(World* world, Entity_Handle entity, Entity_Handle parent_entity);
void copy_transform(World* world, Entity_Handle entity, Entity_Handle copied_entity);

void set_mesh(World* world, Entity_Handle entity, Mesh mesh, Material material);
void copy_mesh(World* world, Entity_Handle entity, Entity_Handle copied_entity);

void set_perspective(World* world,
                     Entity_Handle entity,
                     f32 fov=glm::radians(90.0f),
                     f32 near=0.1f,
                     f32 far=100000.0f,
                     f32 aspect_ratio=1.0f,
                     glm::vec4 viewport=glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

void set_orthographic(World* world,
                      Entity_Handle entity,
                      f32 left,
                      f32 right,
                      f32 top,
                      f32 bottom,
                      f32 near=0,
                      f32 far=0,
                      glm::vec4 viewport=glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

void render_world(World* world);
