
struct Vertex_2D {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texcoord;
    glm::vec3 normal;
};

struct Mesh_Builder{
    usize min_vertex_count;
    usize max_vertex_count;
    usize vertex_stride; // number of floats per vertex
    f32* vertex_buffer;
    f32* vertex_buffer_data_at;

    usize min_index_count;
    usize max_index_count;
    u16* indices_buffer;
    u16* indices_buffer_data_at;
};

void
initialze_mesh_data(usize min_vertex_count, usize vertex_stride, usize min_index_count);

void
write_3d_quad(Mesh_Builder* data, const Vertex& v0, Vertex v1, Vertex v2, Vertex v3);
