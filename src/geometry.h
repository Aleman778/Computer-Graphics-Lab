
struct Vertex_2D {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texcoord;
    glm::vec3 normal;
};

struct Mesh_Builder {
    std::vector<Vertex> vertices;
    std::vector<u16> indices;
};

inline void push_quad(Mesh_Builder* mb, const Vertex& v0, Vertex v1, Vertex v2, Vertex v3);
inline void push_quad(Mesh_Builder* mb,
                      glm::vec3 p1, glm::vec2 t1, glm::vec3 n1,
                      glm::vec3 p2, glm::vec2 t2, glm::vec3 n2,
                      glm::vec3 p3, glm::vec2 t3, glm::vec3 n3,
                      glm::vec3 p4, glm::vec2 t4, glm::vec3 n4);

void push_cuboid_mesh(Mesh_Builder* mb, glm::vec3 c, glm::vec3 d);
