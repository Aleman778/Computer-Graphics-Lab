
struct Mesh {
    GLuint  vbo;
    GLuint  ibo;
    GLuint  vao;
    GLsizei count;
    GLenum  mode; // e.g. GL_TRIANGLES
};

// NOTE(alexander): this defines the location index that all shaders should use!
enum Layout_Type {
    Layout_Positions,  // 0
    Layout_Texcoords,  // 1
    Layout_Normals,    // 2
    Layout_Colors,     // 3
};

struct Mesh_Layout {
    Layout_Type location; // for simplicity always use the same index for all shaders.
    GLint       size;
    GLenum      type;
    GLboolean   normalized;
    GLsizei     stride;
    usize       offset; // aka. pointer which makes no sense.
};

struct Texture {
    GLenum target; // e.g. GL_TEXTURE_2D
    GLuint handle;
};

struct Shader_Base {
    GLuint program;
    GLint u_model_transform;
    GLint u_mvp_transform;
    GLint u_sky_color; // usually same as clear color
    GLint u_fog_density; // increase density -> more fog (shorter view distance)
    GLint u_fog_gradient; // increase gradient -> sharper transition
    struct {
        GLint position;
        GLint view_position;
        GLint color;
        GLint ambient_intensity;
    } u_light_setup;
};

struct Basic_2D_Shader {
    Shader_Base base;
    GLint u_color;
};

struct Basic_Shader {
    Shader_Base base;
    GLint u_color;
    GLint u_light_attenuation;
    GLint u_light_intensity;
};

struct Phong_Shader {
    Shader_Base base;
    GLint u_diffuse_color;
    GLint u_diffuse;
    GLint u_specular;
    GLint u_shininess;
};

struct Light_Setup {
    glm::vec3 color;
    glm::vec3 position;
    glm::vec3 view_position;
    float ambient_intensity;
};

enum Material_Type {
    Material_Type_Basic,
    Material_Type_Phong,
};

struct Basic_Material {
    Basic_Shader* shader;
    glm::vec4 color;
};

struct Phong_Material {
    Phong_Shader* shader;
    glm::vec3 diffuse_color;
    Texture* diffuse_map;
    Texture* specular_map;
    f32 shininess;
};

struct Material {
    Material_Type type;
    union {
        Basic_Material Basic;
        Phong_Material Phong;
    };
    Shader_Base* shader;
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
    Material material;
    Transform transform;
};

struct Camera_3D {
    f32 fov;
    f32 near;
    f32 far;
    f32 aspect_ratio;
    Transform transform;
    glm::mat4 perspective_matrix; // perspective * view (aka. camera transform)
    glm::mat4 combined_matrix; // perspective * view (or camera transform matrix)
    bool is_dirty; // does projection and combined matrix need to be updated?
};

struct Fps_Camera {
    Camera_3D base;
    f32 rotation_x;
    f32 rotation_y;
    f32 sensitivity;
};

struct Camera_2D {
    f32 offset_x;
    f32 offset_y;
    f32 x;
    f32 y;
    f32 zoom;
};

Mesh create_cuboid_basic_mesh(glm::vec3 c, glm::vec3 d);
Mesh create_cuboid_mesh(glm::vec3 c, glm::vec3 d);

inline void apply_shader(Shader_Base* shader,
                         Light_Setup* light_setup=NULL,
                         glm::vec3 sky_color=glm::vec3(0.0f),
                         f32 fog_desnity=0.0f,
                         f32 fog_gradient=0.0f);
inline void apply_basic_shader(Basic_Shader* shader, f32 light_intensity, f32 light_attenuation);

void initialize_transform(Transform* transform,
                          glm::vec3 pos=glm::vec3(0.0f),
                          glm::quat rot=glm::quat(glm::vec3(0.0f, 0.0f, 0.0f)),
                          glm::vec3 scale=glm::vec3(1.0f));
void update_transform(Transform* transform);

void draw_mesh();
void draw_graphics_node(Graphics_Node* node, Camera_3D* camera);

void initialize_camera_3d(Camera_3D* camera,
                          f32 fov=glm::radians(90.0f),
                          f32 near=0.1f,
                          f32 far=100000.0f,
                          f32 aspect_ratio=1.0f);

void update_camera_3d(Camera_3D* camera, f32 aspect_ratio);

void initialize_fps_camera(Fps_Camera* camera,
                           f32 fov=glm::radians(90.0f),
                           f32 near=0.1f,
                           f32 far=100000.0f,
                           f32 senitivity=0.01f,
                           f32 aspect_ratio=1.0f);

void update_fps_camera(Fps_Camera* camera, Input* input, int width, int height);

void update_camera_2d(Camera_2D* camera, Input* input);

void begin_scene(const glm::vec4& clear_color, const glm::vec4& viewport, bool depth_testing=false);

void end_scene();

Texture generate_white_2d_texture();
Texture load_2d_texture_from_file(const char* filepath,
                                  bool gen_mipmaps=true,
                                  f32 mipmap_bias=-0.8f,
                                  bool use_anisotropic_filtering=true, // requires gen_mipmaps=true
                                  f32 max_anisotropy=4.0f);

Basic_2D_Shader compile_basic_2d_shader();
Basic_Shader compile_basic_shader();
Phong_Shader compile_phong_shader();
