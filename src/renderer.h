
struct Mesh {
    GLuint vbo;
    GLuint ibo;
    GLuint vao;
    GLsizei count;
};

struct Basic_Shader {
    GLuint program;
    GLint u_transform;
    GLint u_color;
    GLint u_light_attenuation;
    GLint u_light_intensity;
};

struct Basic_Material {
    Basic_Shader* shader;
    glm::vec3 color;
};

struct Transform {
    glm::vec3 local_position;
    glm::quat local_rotation;
    glm::vec3 local_scale;
    glm::mat4 matrix; // cached transform calculation
    bool is_dirty; // does matrix need to be updated?
};

struct Graphics_Node {
    Mesh* mesh;
    Basic_Material material;
    Transform transform;
};

struct Camera3D {
    f32 fov;
    f32 near;
    f32 far;
    f32 aspect_ratio;
    Transform transform;
    glm::mat4 perspective_matrix; // perspective * view (aka. camera transform)
    glm::mat4 combined_matrix; // perspective * view (or camera transform matrix)
    bool is_dirty; // does projection and combined matrix need to be updated?
};

struct Camera2D {
    f32 offset_x;
    f32 offset_y;
    f32 x;
    f32 y;
    f32 zoom;
};

Mesh create_cube_mesh(glm::vec3 c, glm::vec3 d);

inline void apply_basic_shader(Basic_Shader* material, f32 light_intensity, f32 light_attenuation);

void update_basic_material(Basic_Material* material);

void initialize_transform(Transform* transform,
                          glm::vec3 pos=glm::vec3(0.0f),
                          glm::quat rot=glm::quat(glm::vec3(0.0f, 0.0f, 0.0f)),
                          glm::vec3 scale=glm::vec3(1.0f));
void update_transform(Transform* transform);

void draw_graphics_node(Graphics_Node* mesh);

void initialize_camera_3d(Camera3D* camera, f32 near=0.1f, f32 far=10000.0f, f32 aspect_ratio=1.0f);
void update_camera_3d(Camera3D* camera, f32 aspect_ratio);
    
void update_camera_2d(Camera2D* camera, f32 mouse_x, f32 mouse_y, f32 zoom, bool pan);

void begin_3d_scene(const glm::vec4& clear_color, const glm::vec4& viewport);

void end_3d_scene();

GLuint load_shader_program_from_sources(const char* vertex_shader, const char* fragment_shader);

Basic_Shader compile_basic_material_shader();
