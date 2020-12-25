
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

inline void
push_quad(Mesh_Builder* mb, const Vertex& v0, Vertex v1, Vertex v2, Vertex v3) {
    assert(mb->vertices.size() < 65536 && "running out of indices");
    u16 base_index = (u16) mb->vertices.size();
    mb->vertices.push_back(v0);
    mb->vertices.push_back(v1);
    mb->vertices.push_back(v2);
    mb->vertices.push_back(v3);
    mb->indices.push_back(base_index);
    mb->indices.push_back(base_index + 1);
    mb->indices.push_back(base_index + 2);
    mb->indices.push_back(base_index);
    mb->indices.push_back(base_index + 2);
    mb->indices.push_back(base_index + 3);
}

inline void
push_quad(Mesh_Builder* mb,
           glm::vec3 p1, glm::vec2 t1, glm::vec3 n1,
           glm::vec3 p2, glm::vec2 t2, glm::vec3 n2,
           glm::vec3 p3, glm::vec2 t3, glm::vec3 n3,
           glm::vec3 p4, glm::vec2 t4, glm::vec3 n4) {
    push_quad(mb, {p1, t1, n1}, {p2, t2, n2}, {p3, t3, n3}, {p4, t4, n4});
}

void
push_cuboid_mesh(Mesh_Builder* mb, glm::vec3 c, glm::vec3 d) {
    d.x /= 2.0f; d.y /= 2.0f; d.z /= 2.0f;

    mb->vertices.reserve(4*6);
    mb->indices.reserve(6*6);

    // Front
    push_quad(mb,
              glm::vec3(c.x - d.x, c.y - d.y, c.z - d.z), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
              glm::vec3(c.x + d.x, c.y - d.y, c.z - d.z), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
              glm::vec3(c.x + d.x, c.y + d.y, c.z - d.z), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
              glm::vec3(c.x - d.x, c.y + d.y, c.z - d.z), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f));

    // Back
    push_quad(mb,
              glm::vec3(c.x - d.x, c.y - d.y, c.z + d.z), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
              glm::vec3(c.x - d.x, c.y + d.y, c.z + d.z), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
              glm::vec3(c.x + d.x, c.y + d.y, c.z + d.z), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f),
              glm::vec3(c.x + d.x, c.y - d.y, c.z + d.z), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // Left
    push_quad(mb,
              glm::vec3(c.x - d.x, c.y - d.y, c.z - d.z), glm::vec2(0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
              glm::vec3(c.x - d.x, c.y + d.y, c.z - d.z), glm::vec2(1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
              glm::vec3(c.x - d.x, c.y + d.y, c.z + d.z), glm::vec2(1.0f, 1.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
              glm::vec3(c.x - d.x, c.y - d.y, c.z + d.z), glm::vec2(0.0f, 1.0f), glm::vec3(-1.0f, 0.0f, 0.0f));

    // Right
    push_quad(mb,
              glm::vec3(c.x + d.x, c.y - d.y, c.z - d.z), glm::vec2(0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y - d.y, c.z + d.z), glm::vec2(1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y + d.y, c.z + d.z), glm::vec2(1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y + d.y, c.z - d.z), glm::vec2(0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Top
    push_quad(mb,
              glm::vec3(c.x - d.x, c.y + d.y, c.z - d.z), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y + d.y, c.z - d.z), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y + d.y, c.z + d.z), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
              glm::vec3(c.x - d.x, c.y + d.y, c.z + d.z), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Bottom
    push_quad(mb,
              glm::vec3(c.x - d.x, c.y - d.y, c.z - d.z), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
              glm::vec3(c.x - d.x, c.y - d.y, c.z + d.z), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y - d.y, c.z + d.z), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f),
              glm::vec3(c.x + d.x, c.y - d.y, c.z - d.z), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
}

void
push_sphere(Mesh_Builder* mb, glm::vec3 c, f32 r, int detail_x=16, int detail_y=16) {
    detail_y *= 2;

    usize base_index = mb->vertices.size();
    for (int i = 0; i <= detail_x; i++) {
        for (int j = 0; j <= detail_y; j++) {
            f32 phi   = pi     * ((f32) i/(f32) detail_x) - half_pi;
            f32 theta = two_pi * ((f32) j/(f32) detail_y);

            glm::vec3 normal = glm::normalize(glm::vec3(sin(theta)*cos(phi), sin(phi), cos(theta)*cos(phi)));
            glm::vec2 texcoord(1.0f - 0.5f + atan2(normal.x, -normal.z)/two_pi, 0.5f - asin(normal.y)/pi);
            glm::vec3 pos = normal*r + c;
            if (j == detail_y) texcoord.x = 0.0f; // fixes texture seam
            Vertex v = { pos, texcoord, normal };
            mb->vertices.push_back(v);

            if (i > 0 && j > 0) {
                mb->indices.push_back(base_index + (detail_y + 1)*i       + j - 1);
                mb->indices.push_back(base_index + (detail_y + 1)*(i - 1) + j - 1);
                mb->indices.push_back(base_index + (detail_y + 1)*i       + j);
                
                mb->indices.push_back(base_index + (detail_y + 1)*i       + j);
                mb->indices.push_back(base_index + (detail_y + 1)*(i - 1) + j - 1);
                mb->indices.push_back(base_index + (detail_y + 1)*(i - 1) + j);
            }
        }
    }
}

void
push_circle_triangle_strips(Mesh_Builder* mb, glm::vec3 c, glm::vec3 n, f32 r, int detail=16) {
    Vertex center_v = { c, glm::vec2(0.5f, 0.5f), n };
    usize center_index = mb->vertices.size();
    mb->vertices.push_back(center_v);

    // build bottom circle
    for (int i = 0; i < detail; i++) {
        f32 angle = two_pi - two_pi * ((f32) i/ (f32) detail);
        glm::vec3 pos(c.x + cos(angle)*r, c.y, c.z + sin(angle)*r);
        Vertex v = { pos, glm::vec2(cos(angle), sin(angle)), n };
        mb->vertices.push_back(v);
        mb->indices.push_back(center_index + 1 + i);
        mb->indices.push_back(center_index);
    }
    mb->indices.push_back(center_index + 1);
}

void
push_cylinder_triangle_strips(Mesh_Builder* mb, glm::vec3 bc, f32 r, f32 h, int detail=16) {
    glm::vec3 tc = glm::vec3(bc.x, bc.y - h, bc.z);

    push_circle_triangle_strips(mb, bc, glm::vec3(0.0f, -1.0f, 0.0f), r, detail);
    usize base_index = mb->vertices.size();

    for (int i = 0; i <= detail; i++) {
        f32 angle = two_pi - two_pi * ((f32) i/ (f32) detail);
        glm::vec3 normal = glm::normalize(glm::vec3(cos(angle), 0.0f, sin(angle)));
        glm::vec3 bottom_pos = normal*r + bc;
        glm::vec2 bottom_texcoord(cos(angle), 0.0f);
        glm::vec3 top_pos = normal*r + tc;
        glm::vec2 top_texcoord(cos(angle), h);

        Vertex bv = { bottom_pos, bottom_texcoord, normal };
        mb->vertices.push_back(bv);

        Vertex tv = { top_pos, top_texcoord, normal };
        mb->vertices.push_back(tv);

        mb->indices.push_back(base_index + i*2);
        mb->indices.push_back(base_index + i*2 + 1);
    }

    push_circle_triangle_strips(mb, tc, glm::vec3(0.0f, 1.0f, 0.0f), r, detail);
}

void
push_cone_triangle_strips(Mesh_Builder* mb, glm::vec3 bc, f32 r, f32 h, int detail=16) {
    push_circle_triangle_strips(mb, bc, glm::vec3(0.0f, -1.0f, 0.0f), r, detail);

    usize top_index = mb->vertices.size();
    Vertex top_v = { glm::vec3(bc.x, bc.y + h, bc.z), glm::vec2(0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f) };
    mb->vertices.push_back(top_v);

    for (int i = 0; i < detail; i++) {
        f32 angle = two_pi - two_pi * ((f32) i/ (f32) detail);
        glm::vec3 pos(bc.x + cos(angle)*r, bc.y, bc.z + sin(angle)*r);
        glm::vec3 normal(pos.x*h/r, r/h, pos.z*h/r);
        normal = glm::normalize(normal);
        Vertex v = { pos, glm::vec2(cos(angle), sin(angle)), normal };
        mb->vertices.push_back(v);
        mb->indices.push_back(top_index + i + 1);
        mb->indices.push_back(top_index);
    }
    mb->indices.push_back(top_index + 1);
}

void
push_flat_plane(Mesh_Builder* mb,
                glm::vec3 pt,
                f32 width,
                f32 height,
                glm::vec3 normal=glm::vec3(0.0f, 1.0f, 0.0f),
                int detail_x=100,
                int detail_y=100) {

    f32 dw = width/(f32) detail_x;
    f32 dh = height/(f32) detail_y;
    usize base_index = mb->vertices.size();

    for (int i = 0; i < detail_x; i++) {
        for (int j = 0; j < detail_y; j++) {
            glm::vec3 pos = glm::vec3(pt.x + i*dw, pt.y, pt.z + j*dh);
            Vertex v = { pos, glm::vec2(i, j), normal };
            mb->vertices.push_back(v);

            // push quad
            if (i != detail_x - 1 && j != detail_y - 1) {
                mb->indices.push_back(base_index + j + i * detail_y);
                mb->indices.push_back(base_index + j + i * detail_y + 1);
                mb->indices.push_back(base_index + j + (i + 1) * detail_y);

                mb->indices.push_back(base_index + j + i * detail_y + 1);
                mb->indices.push_back(base_index + j + (i + 1) * detail_y + 1);
                mb->indices.push_back(base_index + j + (i + 1) * detail_y);
            }
        }
    }
}
