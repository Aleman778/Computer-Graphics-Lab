

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

    for (int i = 0; i <= detail_x; i++) {
        for (int j = 0; j < detail_y; j++) {
            if ((i != 0 && i != detail_x) || j == 0) {
                f32 phi   = pi * ((f32) i/(f32) detail_x) + half_pi;
                f32 theta = two_pi * ((f32) j/(f32) detail_y);

                glm::vec3 normal = glm::normalize(glm::vec3(sin(theta)*cos(phi), cos(theta)*cos(phi), sin(phi)));
                glm::vec2 texcoord(0.5f + atan2(normal.z, normal.x)/two_pi, 0.5f - asin(normal.y)/pi);

                // TODO(alexander): attemt to fix the ugly seam, improve this
                if (i == detail_x/2 - 1 && j < detail_y/2) {
                    texcoord.x = 0.679945 - 0.16666 + 0.16666 * ((f32) j/(f32) detail_y/2);
                }
                if (i == detail_x/2 && j < detail_y/2) {
                    texcoord.x = 0.679945 - 0.16666*2 + 0.16666 * ((f32) j/(f32) detail_y/2);
                }

                glm::vec3 pos = normal*r + c;
                Vertex v = { pos, texcoord, normal };
                mb->vertices.push_back(v);
            }
            
            if (i == 1) {
                mb->indices.push_back(0);
                mb->indices.push_back(j + 1);
            } else if (i == detail_x) {
                mb->indices.push_back(mb->vertices.size() + j - 1 - detail_y);
                mb->indices.push_back(mb->vertices.size() - 1);
            } else if (i >= 2) {
                mb->indices.push_back(detail_y*(i - 2) + j + 1);
                mb->indices.push_back(detail_y*(i - 1) + j + 1);
            }
        }
    }

    int arr_index = 0;
    for (auto idx : mb->indices) {
        if (idx >= mb->vertices.size()) {
            printf("Found invalid index `%u` at mb->indices[%d]\n", idx, arr_index);
        }
        arr_index++;
    }
}


void
push_cylinder_triangle_strips(Mesh_Builder* mb, glm::vec3 c, f32 r, f32 h, int detail=16) {
    
     
}
