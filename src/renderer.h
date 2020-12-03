
struct Mesh {
    GLuint vbo;
    GLuint ibo;
    GLuint vao;
    GLsizei count;
};

struct Camera2D {
    f32 offset_x;
    f32 offset_y;
    f32 x;
    f32 y;
    f32 zoom;
};

Mesh create_cube_mesh(glm::vec3 c, glm::vec3 d);

void update_camera_2d(Camera2D* camera, f32 mouse_x, f32 mouse_y, f32 zoom, bool pan);

GLuint load_glsl_shader_from_sources(const char* vertex_shader, const char* fragment_shader);

GLuint load_glsl_shader_from_file(const char* filepath);
