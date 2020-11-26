
struct Triangle {
    uint v[3]; // vertex indices to each edge
    Triangle* n[3]; // neighboring triangles
    usize index; // first index in triangulation index vector
};

struct Node {
    glm::vec2 ray; // ray is vector pointing out from origin point
    glm::vec2 origin; // origin point
    Triangle* triangle; // for non-leaf nodes this is NULL
    Node* children[3]; // can either be binary or trinary node
    int children_count;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Triangulation {
    std::vector<Vertex> vertices;
    std::vector<uint> indices;
    Node* root;
};

struct Triangulation_Scene {
    GLuint vbo;
    GLuint ibo;
    GLuint vao;
    GLsizei vertex_count;
    GLsizei index_count;

    GLuint shader;
    GLint  tint_color_uniform;
    GLint  transform_uniform;

    std::unordered_set<glm::vec2> points;
    Triangulation triangulation;

    Camera2D camera;

    std::mt19937 rng;

    glm::vec4 primary_color;
    glm::vec4 secondary_color;
    glm::vec4 colors[4]; // used in 4-coloring mode

    bool show_gui;

    int picking_option;
    int coloring_option;

    bool is_initialized;
};

static inline f32
triangle_area(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static inline void
push_back_triangle(Triangulation* triangulation, Triangle* t) {
    t->index = triangulation->indices.size();
    triangulation->indices.push_back(t->v[0]);
    triangulation->indices.push_back(t->v[1]);
    triangulation->indices.push_back(t->v[2]);
}

static void
build_convex_hull(const std::unordered_set<glm::vec2>& point_set, std::vector<glm::vec2>& result) {
    std::vector<glm::vec2> points = std::vector<glm::vec2>(point_set.begin(), point_set.end());
    std::sort(points.begin(), points.end(), [](glm::vec2 v1, glm::vec2 v2) -> bool {
        if (v1.x == v2.x) return v1.y < v2.y;
        return v1.x < v2.x;
    });

    for (int j = 0; j < 2; j++) {
        usize result_base = result.size();
        for (int i = 0; i < points.size(); i++) {
            glm::vec2 q = points[j == 0 ? i : points.size() - i - 1]; // query point
            while (result.size() >= result_base + 2) {
                f32 area = triangle_area(result[result.size() - 2], result[result.size() - 1], q);
                if (area > 0) {
                    result.pop_back();
                } else {
                    break;
                }
            }
            result.push_back(q);
        }
        if (result.size() > 0) result.pop_back(); // NOTE(alexander): remove duplicate edge
    }
}

static Node*
build_fan_triangulation(const std::vector<glm::vec2>& convex_hull, Triangulation* triangulation) {
    int num_tris = (int) convex_hull.size() - 2;
    if (num_tris <= 0) return new Node();

    triangulation->vertices.reserve(convex_hull.size());
    for (int i = 0; i < convex_hull.size(); i++) {
        Vertex v = { convex_hull[i], primary_fg_color };
        triangulation->vertices.push_back(v);
    }

    std::vector<Triangle*> tris(num_tris);
    for (int i = 0; i < num_tris; i++) {
        Triangle* t = new Triangle();
        tris[i] = t;
        t->v[0] = 0; t->v[1] = i + 1; t->v[2] = i + 2;
        if (i > 0) {
            t->n[0] = tris[i - 1];
            tris[i - 1]->n[2] = t;
        }
        push_back_triangle(triangulation, t);
    }

    glm::vec2 p0 = convex_hull[0];
    std::function<Node*(int,int)> build_search_tree;
    build_search_tree = [p0, tris, triangulation, &build_search_tree](int beg, int end) -> Node* {
        if (end - beg <= 0) return NULL;
        Node* node = new Node();
        node->origin = p0;
        node->ray = triangulation->vertices[tris[beg]->v[1]].pos;
        if (end - beg == 1) {
            node->triangle = tris[beg];
            return node;
        }
        int split = (end - beg)/2;
        Node* left = build_search_tree(beg, beg + split);
        Node* right = build_search_tree(beg + split, end);
        node->children[0] = left; node->children[1] = right;
        node->children_count = 2;
        return node;
    };

    triangulation->root = build_search_tree(0, num_tris);
    return triangulation->root;
}

static void // TODO(alexander): check if we are inside the convex hull, or a found triangle?
point_location(Node* node, glm::vec2 p, std::vector<Node*>* result) {
    if (!node) return;
    if (node->triangle) {
        result->push_back(node);
        return;
    }
    if (node->children_count <= 1) return;

    glm::vec2 origin = node->children[0]->origin;

    if (node->children_count == 2) {
        f32 d0 = triangle_area(origin, p, node->children[0]->ray);
        f32 d1 = triangle_area(origin, p, node->children[1]->ray);
        f32 area = triangle_area(origin, p, node->children[1]->ray);
        if (d0 >= 0 && d1 <= 0) {
            point_location(node->children[0], p, result);
        }
        if (d1 >= 0) {
            point_location(node->children[1], p, result);
        }

    } else if (node->children_count == 3) {
        f32 d0 = triangle_area(origin, p, node->children[0]->ray);
        f32 d1 = triangle_area(origin, p, node->children[1]->ray);
        f32 d2 = triangle_area(origin, p, node->children[2]->ray);
        if (d0 >= 0 && d1 <= 0) {
            point_location(node->children[0], p, result);
        }
        if (d1 >= 0 && d2 <= 0) {
            point_location(node->children[1], p, result);
        }
        if (d2 >= 0 && d0 <= 0) {
            point_location(node->children[2], p, result);
        }
    }
}

static void // NOTE(alexander): left, right, opposite are triangle vertex indices [0 - 2]
split_triangle_at_edge(Triangulation* triangulation,
                       Node* node,
                       Node* neighbor,
                       glm::vec2 p,
                       int left,
                       int right,
                       int opposite) {

    Triangle* t  = node->triangle;
    Triangle* t1 = new Triangle();
    Triangle* t2 = new Triangle();
    Node* n1 = new Node();
    Node* n2 = new Node();

    uint pv_index = (uint) triangulation->vertices.size();
    Vertex pv = { p, primary_fg_color };
    triangulation->vertices.push_back(pv);

    n1->origin = p;
    n1->triangle = t1;
    n2->origin = p;
    n2->triangle = t2;
    node->children[0] = n1;
    node->children[1] = n2;
    node->children_count = 2;

    t1->v[0] = pv_index;       t1->v[1] = t->v[right];    t1->v[2] = t->v[opposite];
    t2->v[0] = pv_index;       t2->v[1] = t->v[opposite]; t2->v[2] = t->v[left];
    t1->n[0] = t->n[left]; t1->n[1] = t->n[right];    t1->n[2] = t2;
    t2->n[0] = t1;         t2->n[1] = t->n[opposite]; t2->n[2] = t->n[left];
    n1->ray = triangulation->vertices[t->v[right]].pos;
    n2->ray = triangulation->vertices[t->v[opposite]].pos;

    Triangle* nt1 = t->n[right];
    Triangle* nt2 = t->n[opposite];
    if (nt1) {
        for (int i = 0; i < 3; i++) {
            if (nt1->n[i] == t) nt1->n[i] = t1;
        }
    }
    if (nt2) {
        for (int i = 0; i < 3; i++) {
            if (nt2->n[i] == t) nt2->n[i] = t2;
        }
    }

    // NOTE(alexander): removing old triangle, reusing indices for one of the triangles.
    t1->index = t->index;
    triangulation->indices[t->index]     = t1->v[0];
    triangulation->indices[t->index + 1] = t1->v[1];
    triangulation->indices[t->index + 2] = t1->v[2];
    push_back_triangle(triangulation, t2);
    node->triangle = NULL;
    delete t;

    // Now split the neighboring triangle, if any was found
    if (neighbor) {
        t = neighbor->triangle;
        Triangle* t3 = new Triangle();
        Triangle* t4 = new Triangle();
        Node* n3 = new Node();
        Node* n4 = new Node();

        glm::vec2 p0 = triangulation->vertices[t->v[0]].pos;
        glm::vec2 p1 = triangulation->vertices[t->v[1]].pos;
        glm::vec2 p2 = triangulation->vertices[t->v[2]].pos;
        if (triangle_area(p0, p1, p) == 0) {
            left = 0; right = 1; opposite = 2;
        } else if (triangle_area(p1, p2, p) == 0) {
            left = 1; right = 2; opposite = 0;
        } else if (triangle_area(p2, p0, p) == 0) {
            left = 2; right = 0; opposite = 1;
        }

        n3->origin = p;
        n3->triangle = t3;
        n4->origin = p;
        n4->triangle = t4;
        neighbor->children[0] = n3;
        neighbor->children[1] = n4;
        neighbor->children_count = 2;

        t3->v[0] = pv_index; t3->v[1] = t->v[right];    t3->v[2] = t->v[opposite];
        t4->v[0] = pv_index; t4->v[1] = t->v[opposite]; t4->v[2] = t->v[left];
        t3->n[0] = t2; t3->n[1] = t->n[right];    t3->n[2] = t4;
        t4->n[0] = t3; t4->n[1] = t->n[opposite]; t4->n[2] = t1;
        n3->ray = triangulation->vertices[t->v[right]].pos;
        n4->ray = triangulation->vertices[t->v[opposite]].pos;

        // NOTE(alexander): connect first two triangles to these two
        t1->n[0] = t4;
        t2->n[2] = t3;

        nt1 = t->n[right];
        nt2 = t->n[opposite];
        if (nt1) {
            for (int i = 0; i < 3; i++) {
                if (nt1->n[i] == t) nt1->n[i] = t3;
            }
        }
        if (nt2) {
            for (int i = 0; i < 3; i++) {
                if (nt2->n[i] == t) nt2->n[i] = t4;
            }
        }

        // NOTE(alexander): removing old triangle, reusing indices for one of the triangles.
        t3->index = t->index;
        triangulation->indices[t->index]     = t3->v[0];
        triangulation->indices[t->index + 1] = t3->v[1];
        triangulation->indices[t->index + 2] = t3->v[2];
        push_back_triangle(triangulation, t4);
        neighbor->triangle = NULL;
        delete t;
    }
}

static void
split_triangle_at_point(Triangulation* triangulation, glm::vec2 p) {
    std::vector<Node*> nodes;
    point_location(triangulation->root, p, &nodes);
    if (nodes.size() == 0) return;
    Node* node = nodes[0];
    Node* neighbor = NULL;
    Triangle* t  = node->triangle;
    if (nodes.size() > 1) neighbor = nodes[1];
    assert(nodes.size() <= 2);
    assert(node->triangle && "not a leaf node");

    // NOTE(alexander): 3 literal edge case - splits into two triangles
    glm::vec2 p0 = triangulation->vertices[t->v[0]].pos;
    glm::vec2 p1 = triangulation->vertices[t->v[1]].pos;
    glm::vec2 p2 = triangulation->vertices[t->v[2]].pos;
    if (triangle_area(p0, p1, p) == 0) {
        split_triangle_at_edge(triangulation, node, neighbor, p, 0, 1, 2);
    } else if (triangle_area(p1, p2, p) == 0) {
        split_triangle_at_edge(triangulation, node, neighbor, p, 1, 2, 0);
    } else if (triangle_area(p2, p0, p) == 0) {
        split_triangle_at_edge(triangulation, node, neighbor, p, 2, 0, 1);
    } else {

        // NOTE(alexander): common case - splits into three triangles
        uint pv_index = (uint) triangulation->vertices.size();
        triangulation->vertices.push_back({ p, primary_fg_color });

        Triangle* t1 = new Triangle();
        Triangle* t2 = new Triangle();
        Triangle* t3 = new Triangle();
        Node* n1 = new Node();
        Node* n2 = new Node();
        Node* n3 = new Node();

        n1->origin = p;
        n1->triangle = t1;
        n2->origin = p;
        n2->triangle = t2;
        n3->origin = p;
        n3->triangle = t3;

        node->children[0] = n1;
        node->children[1] = n2;
        node->children[2] = n3;
        node->children_count = 3;

        t1->v[0] = t->v[0]; t1->v[1] = t->v[1], t1->v[2] = pv_index;
        t2->v[0] = t->v[1]; t2->v[1] = t->v[2]; t2->v[2] = pv_index;
        t3->v[0] = t->v[2]; t3->v[1] = t->v[0]; t3->v[2] = pv_index;
        t1->n[0] = t->n[0]; t1->n[1] = t2; t1->n[2] = t3;
        t2->n[0] = t->n[1]; t2->n[1] = t3; t2->n[2] = t1;
        t3->n[0] = t->n[2]; t3->n[1] = t1; t3->n[2] = t2;

        n1->ray = triangulation->vertices[t->v[0]].pos;
        n2->ray = triangulation->vertices[t->v[1]].pos;
        n3->ray = triangulation->vertices[t->v[2]].pos;

        node->children[2] = n3;
        node->children_count = 3;

        Triangle* nt1 = t->n[0];
        Triangle* nt2 = t->n[1];
        Triangle* nt3 = t->n[2];
        if (nt1) {
            for (int i = 0; i < 3; i++) {
                if (nt1->n[i] == t) nt1->n[i] = t1;
            }
        }
        if (nt2) {
            for (int i = 0; i < 3; i++) {
                if (nt2->n[i] == t) nt2->n[i] = t2;
            }
        }
        if (nt3) {
            for (int i = 0; i < 3; i++) {
                if (nt3->n[i] == t) nt3->n[i] = t3;
            }
        }

        // NOTE(alexander): removing old triangle, reusing indices for one of the triangles.
        t1->index = t->index;
        triangulation->indices[t->index]     = t1->v[0];
        triangulation->indices[t->index + 1] = t1->v[1];
        triangulation->indices[t->index + 2] = t1->v[2];
        push_back_triangle(triangulation, t2);
        push_back_triangle(triangulation, t3);
        node->triangle = NULL;
        delete t;
    }
}

static Triangulation
build_triangulation_of_points(const std::unordered_set<glm::vec2>& points, std::mt19937& rng) {
    std::vector<glm::vec2> convex_hull;
    build_convex_hull(points, convex_hull);
    std::unordered_set<glm::vec2> convex_hull_set(convex_hull.begin(), convex_hull.end());

    Triangulation triangulation = {};
    build_fan_triangulation(convex_hull, &triangulation);

    for (auto v : points) {
        if (convex_hull_set.find(v) == convex_hull_set.end()) {
            split_triangle_at_point(&triangulation, v);
        }
    }

    return triangulation;
}

static void
destroy_triangulation(Triangulation* tri, Node* node=NULL) {
    if (!tri->root) return;
    if (!node) node = tri->root;

    if (node->triangle) {
        delete node->triangle;
    }

    for (auto child : node->children) {
        if (!child) continue;
        destroy_triangulation(tri, child);
    }

    delete node;
}

static bool
initialize_scene(Triangulation_Scene* scene) {
    // Generate random points
    std::random_device rd;
    scene->rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> dist(10.0f, 710.0f);

    for (int i = 0; i < 64; i++) {
        scene->points.emplace(glm::vec2(290.0f + dist(scene->rng), dist(scene->rng)));
    }

    scene->triangulation = build_triangulation_of_points(scene->points, scene->rng);
    scene->vertex_count = (GLsizei) scene->triangulation.vertices.size();
    scene->index_count = (GLsizei) scene->triangulation.indices.size();

    GLvoid* vdata = 0;
    GLvoid* idata = 0;
    if (scene->vertex_count > 0) vdata = &scene->triangulation.vertices[0];
    if (scene->index_count > 0)  idata = &scene->triangulation.indices[0];

    // Create vertex array object
    glGenVertexArrays(1, &scene->vao);
    glBindVertexArray(scene->vao);

    // Create vertex buffer
    glGenBuffers(1, &scene->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*scene->vertex_count, vdata, GL_STATIC_DRAW);

    // Create index buffer
    glGenBuffers(1, &scene->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint)*scene->index_count, idata, GL_STATIC_DRAW);

    // Setup vertex attributes
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32)*6, 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(f32)*6, (GLvoid*) (sizeof(f32)*2));

    // Done with vertex array
    glBindVertexArray(0);

    const char* vert_shader_source =
        "#version 330\n"
        "layout(location=0) in vec2 pos;\n"
        "layout(location=1) in vec4 color;\n"
        "out vec4 out_color;\n"
        "uniform mat4 transform;\n"
        "void main() {\n"
        "    gl_Position = transform * vec4(pos, 0.0f, 1.0f);\n"
        "    out_color = color;\n"
        "}\n";

    const char* frag_shader_source =
        "#version 330\n"
        "in  vec4 out_color;"
        "out vec4 frag_color;\n"
        "uniform vec4 tint_color;\n"
        "void main() {\n"
        "    frag_color = out_color*tint_color;\n"
        "}\n";

    scene->shader = load_glsl_shader_from_sources(vert_shader_source, frag_shader_source);
    scene->tint_color_uniform = glGetUniformLocation(scene->shader, "tint_color");
    if (scene->tint_color_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `color`");
        return false;
    }

    scene->transform_uniform = glGetUniformLocation(scene->shader, "transform");
    if (scene->transform_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `transform`");
        return false;
    }

    // Initialize colors
    scene->primary_color = primary_fg_color;
    scene->secondary_color = secondary_fg_color;
    scene->colors[0] = red_color;
    scene->colors[1] = green_color;
    scene->colors[2] = blue_color;
    scene->colors[3] = magenta_color;

    // Setup default settings
    scene->coloring_option = 0; // Default
    scene->is_initialized = true;

    return true;
}

static void // NOTE(alexander): assumes correct vertex array object is bound!
update_scene_buffer_data(Triangulation_Scene* scene, bool update_ibo=true) {
    GLvoid* vdata = 0;
    GLvoid* idata = 0;
    scene->vertex_count = (GLsizei) scene->triangulation.vertices.size();
    if (update_ibo) scene->index_count = (GLsizei) scene->triangulation.indices.size();
    else scene->index_count = 0;
    if (scene->vertex_count > 0) vdata = &scene->triangulation.vertices[0];
    if (scene->index_count > 0)  idata = &scene->triangulation.indices[0];
    glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*scene->vertex_count, vdata, GL_STATIC_DRAW);
    if (update_ibo) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint)*scene->index_count, idata, GL_STATIC_DRAW);
    }
}

static void
push_highlight_triangle(Triangulation* triangulation, Triangle* t, glm::vec4 color) {
    glm::vec2 p0 = triangulation->vertices[t->v[0]].pos;
    glm::vec2 p1 = triangulation->vertices[t->v[1]].pos;
    glm::vec2 p2 = triangulation->vertices[t->v[2]].pos;
    uint index = (uint) triangulation->vertices.size();
    triangulation->vertices.push_back({ p0, color });
    triangulation->vertices.push_back({ p1, color });
    triangulation->vertices.push_back({ p2, color });
    triangulation->indices[t->index]     = index;
    triangulation->indices[t->index + 1] = index + 1;
    triangulation->indices[t->index + 2] = index + 2;
}

static void
calculate_distance_coloring(Triangulation_Scene* scene) {
    // Calculate average x and y vertex position
    f32 xm = 0;
    f32 ym = 0;
    for (int i = 0; i < scene->triangulation.vertices.size(); i++) {
        Vertex v = scene->triangulation.vertices[i];
        xm += v.pos.x; ym += v.pos.y;
    }
    xm /= scene->vertex_count; ym /= scene->vertex_count;

    // Calculate maximum distance from any point to average point (xm, ym)
    f32 d = 0;
    for (int i = 0; i < scene->triangulation.vertices.size(); i++) {
        Vertex v = scene->triangulation.vertices[i];
        f32 dsqr = glm::pow(v.pos.x - xm, 2.0f) + glm::pow(v.pos.y - ym, 2.0f);
        if (dsqr > d) d = dsqr;
    }
    d = glm::sqrt(d);

    f32 k = 10.0f; // NOTE(alexander): constant between 1.0f and 20.0f, maybe use ImGui::Slider

    auto red = [xm, ym, d, k](f32 x) -> f32 {
        return (glm::sin(k*glm::pi<f32>()*(d - x)/d) + 1.0f)/2.0f;
    };

    auto green = [xm, ym, d, k](f32 x) -> f32 {
        return (glm::sin(k*glm::pi<f32>()*(d - x)/d + 2*glm::pi<f32>()/3.0f) + 1.0f)/2.0f;
    };

    auto blue = [xm, ym, d, k](f32 x) -> f32 {
        return (glm::sin(k*glm::pi<f32>()*(d - x)/d + 4*glm::pi<f32>()/3.0f) + 1.0f)/2.0f;
    };

    for (int i = 0; i < scene->triangulation.vertices.size(); i++) {
        Vertex* v = &scene->triangulation.vertices[i];
        f32 dv = glm::sqrt(glm::pow(v->pos.x - xm, 2.0f) + glm::pow(v->pos.y - ym, 2.0f));
        v->color.r = red(dv);
        v->color.g = green(dv);
        v->color.b = blue(dv);
    }
    update_scene_buffer_data(scene);
}

static void
calculate_4_coloring(Triangulation_Scene* scene) {
    Triangulation* triangulation = &scene->triangulation;

    // Construct new vertices 
    std::vector<Vertex> vertices;

    std::unordered_map<usize, int> tcolors; // remember triangle colors as index into scene->colors

    std::function<void(Node*)> dfs_4_coloring;
    dfs_4_coloring = [&tcolors, &vertices, &dfs_4_coloring, triangulation, scene](Node* node) {
        if (!node) return;
        Triangle* t = node->triangle;
        if (t) {
            Triangle* t1 = t->n[0];
            Triangle* t2 = t->n[1];
            Triangle* t3 = t->n[2];
            int c1 = -1;
            int c2 = -1;
            int c3 = -1;
            if (t1) {
                auto it = tcolors.find(t1->index);
                c1 = it != tcolors.end() ? it->second : -1;
            }
            if (t2) {
                auto it = tcolors.find(t2->index);
                c2 = it != tcolors.end() ? it->second : -1;
            }
            if (t3) {
                auto it = tcolors.find(t3->index);
                c3 = it != tcolors.end() ? it->second : -1;
            }

            // Find unique color that is not already choosen
            int color = (c1 + 1)%4;
            if (c2 == color) {
                color = (c2 + 1)%4;
                if (c3 == color) color = (color + 1)%4;
                if (c1 == color) color = (color + 1)%4;
            }
            if (c3 == color) {
                color = (c3 + 1)%4;
                if (c1 == color) color = (color + 1)%4;
                if (c2 == color) color = (color + 1)%4;
            }
            tcolors.emplace(t->index, color);

            Vertex v0 = triangulation->vertices[t->v[0]];
            Vertex v1 = triangulation->vertices[t->v[1]];
            Vertex v2 = triangulation->vertices[t->v[2]];
            v0.color = scene->colors[color];
            v1.color = scene->colors[color];
            v2.color = scene->colors[color];
            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
        }

        if (node->children_count > 0) dfs_4_coloring(node->children[0]);
        if (node->children_count > 1) dfs_4_coloring(node->children[1]);
        if (node->children_count > 2) dfs_4_coloring(node->children[2]);
    };

    // Color by depth first search
    dfs_4_coloring(triangulation->root);

    // Update the buffers, set the triangulation vertices
    std::vector<Vertex> old_vertices = triangulation->vertices;
    triangulation->vertices = vertices;
    update_scene_buffer_data(scene, false); // ignore use of ibo
    
    // Restore vertices and index count
    triangulation->vertices = old_vertices;
}

static void // NOTE(alexander): visited contains triangle indices which uniquely identifies one triangle
calculate_extended_picking(Triangulation* triangulation,
                           std::unordered_set<usize>* visited,
                           glm::vec4 highlight_color,
                           Triangle* t,
                           Triangle* curr,
                           Triangle* prev) {
    if (!curr || curr == t || visited->find(curr->index) != visited->end()) return;
    visited->emplace(curr->index);
    
    // Make sure this curr shares at least one edge with t
    if (!(curr->v[0] == t->v[0] || curr->v[0] == t->v[1] || curr->v[0] == t->v[2] ||
          curr->v[1] == t->v[0] || curr->v[1] == t->v[1] || curr->v[1] == t->v[2] ||
          curr->v[2] == t->v[0] || curr->v[2] == t->v[1] || curr->v[2] == t->v[2])) {
        return;
    }
    
    push_highlight_triangle(triangulation, curr, highlight_color);

    int prev_index = 0;
    if      (curr->n[0] == prev) prev_index = 0;
    else if (curr->n[1] == prev) prev_index = 1;
    else if (curr->n[2] == prev) prev_index = 2;
    calculate_extended_picking(triangulation, visited, highlight_color, t, curr->n[(prev_index + 1)%3], curr);
    calculate_extended_picking(triangulation, visited, highlight_color, t, curr->n[(prev_index + 2)%3], curr);
}

void
update_and_render_scene(Triangulation_Scene* scene, Window* window) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    Triangulation* triangulation = &scene->triangulation;

    // Setup orthographic projection and camera transform
    update_camera_2d(window, &scene->camera);
    glm::mat4 transform = glm::ortho(0.0f, (f32) window->width, (f32) window->height, 0.0f, 0.0f, 1000.0f);
    transform = glm::scale(transform, glm::vec3(scene->camera.zoom + 1.0f, scene->camera.zoom + 1.0f, 0));
    transform = glm::translate(transform, glm::vec3(scene->camera.x, scene->camera.y, 0));

    // Bind vertex array
    glBindVertexArray(scene->vao);

    auto set_solid_color = [scene](glm::vec4 color) {
        for (int i = 0; i < scene->triangulation.vertices.size(); i++) {
            scene->triangulation.vertices[i].color = color;
        }
        update_scene_buffer_data(scene);
    };

    auto retriangulate_points = [scene, &set_solid_color]() {
        destroy_triangulation(&scene->triangulation);
        scene->triangulation = build_triangulation_of_points(scene->points, scene->rng);
        switch (scene->coloring_option) {
            case 1: calculate_distance_coloring(scene); break;
            case 2: calculate_4_coloring(scene); break;
            default: set_solid_color(scene->primary_color); break;
        }
    };

    // Mouse interaction
    f32 x = (window->input.mouse_x/(scene->camera.zoom + 1.0f) - scene->camera.x);
    f32 y = (window->input.mouse_y/(scene->camera.zoom + 1.0f) - scene->camera.y);
    if (was_pressed(&window->input.left_mb)) {
        if (window->input.shift_key.ended_down) {
            // Push new vertex interactively
            scene->points.emplace(glm::vec2(x, y));
            retriangulate_points();

        } else {
            // Point location color selected node
            std::vector<Node*> nodes;
            point_location(triangulation->root, glm::vec2(x, y), &nodes);
            if (nodes.size() >= 0) {
                Node* node = nodes[0];
                Triangle* t = node->triangle;

                // Copy vertices and indices, to restore original shape
                std::vector<Vertex> vertices = triangulation->vertices;
                std::vector<uint> indices = triangulation->indices;

                // Create a new vertices that will render the selected triangle
                push_highlight_triangle(triangulation, t, scene->secondary_color);

                if (scene->picking_option == 1) {
                    if (t->n[0]) push_highlight_triangle(triangulation, t->n[0], scene->colors[0]);
                    if (t->n[1]) push_highlight_triangle(triangulation, t->n[1], scene->colors[1]);
                    if (t->n[2]) push_highlight_triangle(triangulation, t->n[2], scene->colors[2]);
                } else if (scene->picking_option == 2) {
                    std::unordered_set<usize> visited;
                    calculate_extended_picking(triangulation, &visited, scene->secondary_color, t, t->n[0], t);
                    calculate_extended_picking(triangulation, &visited, scene->secondary_color, t, t->n[1], t);
                    calculate_extended_picking(triangulation, &visited, scene->secondary_color, t, t->n[2], t);
                }

                // Update buffers
                update_scene_buffer_data(scene);

                // Reset the vertices and indices
                triangulation->vertices = vertices;
                triangulation->indices = indices;
            }
        }
    }

    // Set viewport
    glViewport(0, 0, window->width, window->height);

    // Render and clear background
    glClearColor(primary_bg_color.x, primary_bg_color.y, primary_bg_color.z, primary_bg_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable shader
    glUseProgram(scene->shader);

    // Render `filled` triangulated shape
    glUniformMatrix4fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform4f(scene->tint_color_uniform, 1.0f, 1.0f, 1.0f, 1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (scene->index_count > 0) glDrawElements(GL_TRIANGLES, scene->index_count, GL_UNSIGNED_INT, 0);
    else glDrawArrays(GL_TRIANGLES, 0, scene->vertex_count);

    // Render `outlined` triangulated shape
    glLineWidth(scene->camera.zoom*0.5f + 2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform4f(scene->tint_color_uniform, 0.4f, 0.4f, 0.4f, 1.0f);
    if (scene->index_count > 0) glDrawElements(GL_TRIANGLES, scene->index_count, GL_UNSIGNED_INT, 0);
    else glDrawArrays(GL_TRIANGLES, 0, scene->vertex_count);

    // Render `points` used in triangulated shape
    glUniform4f(scene->tint_color_uniform, 0.4f, 0.4f, 0.4f, 1.0f);
    glPointSize((scene->camera.zoom + 2.0f) + 4.0f);
    if (scene->index_count > 0) glDrawElements(GL_POINTS, scene->index_count, GL_UNSIGNED_INT, 0);
    else glDrawArrays(GL_POINTS, 0, scene->vertex_count);

    // Begin ImGui
    ImGui::Begin("Lab 2 - Triangulation", &scene->show_gui, ImVec2(280, 350), ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Modify Input Points:");
    if (ImGui::Button("Clear Points")) {
        scene->points.clear();
        retriangulate_points();
    }
    if (ImGui::Button("Fixed Points")) {
        scene->points.clear();
        scene->points.emplace(glm::vec2(200,  400));
        scene->points.emplace(glm::vec2(1000, 400));
        scene->points.emplace(glm::vec2(600,  200));
        scene->points.emplace(glm::vec2(800,  200));
        scene->points.emplace(glm::vec2(600,  600));
        scene->points.emplace(glm::vec2(600,  400));
        retriangulate_points();
    }
    if (ImGui::Button("Randomize Points")) {
        scene->points.clear();
        std::uniform_real_distribution<f32> dist(10.0f, 710.0f);
        for (int i = 0; i < 64; i++) {
            scene->points.emplace(glm::vec2(290.0f + dist(scene->rng), dist(scene->rng)));
        }
        retriangulate_points();
    }

    ImGui::Text("Picking options:");
    ImGui::RadioButton("Default picking", &scene->picking_option, 0);
    ImGui::RadioButton("Default picking + neighbors", &scene->picking_option, 1);
    ImGui::RadioButton("Extended picking", &scene->picking_option, 2);
    
    ImGui::Text("Coloring options:");
    if (ImGui::RadioButton("Solid color", &scene->coloring_option, 0)) {
        set_solid_color(scene->primary_color);
    }
    if (ImGui::RadioButton("Coloring based on distance", &scene->coloring_option, 1)) {
        calculate_distance_coloring(scene);
    }
    if (ImGui::RadioButton("4 Coloring", &scene->coloring_option, 2)) {
        calculate_4_coloring(scene);
    }

    ImGui::Text("Info:");
    ImGui::Text("Number of points: %zu", scene->points.size());
    ImGui::Text("Number of triangles: %zu", triangulation->vertices.size()/3);
    ImGui::Text("x = %.3f", x);
    ImGui::Text("y = %.3f", y);
    ImGui::End();

    // Reset states
    glLineWidth(1);
    glPointSize(1);
    glUseProgram(0);
    glBindVertexArray(0);
}
