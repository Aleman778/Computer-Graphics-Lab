
struct Vertex {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Triangle {
    Vertex v[3];
    Triangle* neighbors[3];
};

struct Node {
    Node* parent; // if this is root node, then parent is NULL
    glm::vec2 ray; // ray is vector pointing out from origin point
    glm::vec2 origin; // origin point
    Triangle* triangle; // for non-leaf nodes this is NULL
    Node* children[3]; // can either be binary or trinary node
    int children_count;
};

struct Triangulation {
    std::vector<Vertex> vertices;
    Node* root;
};

struct Triangulation_Scene {
    GLuint vbo;
    GLuint vao;
    GLsizei count;

    GLuint shader;
    GLint  tint_color_uniform;
    GLint  transform_uniform;

    std::unordered_set<glm::vec2> points;
    Triangulation triangulation;

    Camera2D camera;

    std::mt19937 rng;

    bool show_gui;

    int coloring_option;

    bool is_initialized;
};

static inline f32
triangle_area(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static void
build_convex_hull(const std::unordered_set<glm::vec2>& point_set, std::vector<glm::vec2>& result) {
    std::vector<glm::vec2> points = std::vector<glm::vec2>(point_set.begin(), point_set.end());
    std::sort(points.begin(), points.end(), [](glm::vec2 v1, glm::vec2 v2) -> bool {
        if (v1.x == v2.x) return v1.y < v2.y;
        return v1.x < v2.x;
    });

    for (int j = 0; j < 2; j++) {
        int result_base = result.size();
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
build_fan_triangulation(const std::vector<glm::vec2>& convex_hull) {
    int num_tris = convex_hull.size() - 2;
    if (num_tris <= 0) return new Node();

    glm::vec2 p0 = convex_hull[0];
    std::vector<Triangle*> tris(num_tris);
    for (int i = 0; i < num_tris; i++) {
        Triangle* t = new Triangle();
        tris[i] = t;
        t->v[0] = { p0, primary_fg_color };
        t->v[1] = { convex_hull[i + 1], primary_fg_color };
        t->v[2] = { convex_hull[i + 2], primary_fg_color };
        if (i > 0) {
            t->neighbors[0]           = tris[i - 1];
            tris[i - 1]->neighbors[2] = t;
        }
    }

    std::function<Node*(int,int)> build_search_tree;
    build_search_tree = [p0, tris, &build_search_tree](int beg, int end) -> Node* {
        if (end - beg <= 0) return NULL;
        Node* node = new Node();
        node->origin = p0;
        node->ray = tris[beg]->v[1].pos;
        if (end - beg == 1) {
            node->triangle = tris[beg];
            return node;
        }
        int split = (end - beg)/2;
        Node* left = build_search_tree(beg, beg + split);
        Node* right = build_search_tree(beg + split, end);
        node->children[0] = left;
        node->children[1] = right;
        node->children_count = 2;
        return node;
    };

    return build_search_tree(0, num_tris);
}

static void
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

static void
split_triangle_at_edge(Node* node, Node* neighbor, glm::vec2 p, int left, int right, int opposite) {
    Triangle* t  = node->triangle;
    Triangle* t1 = new Triangle();
    Triangle* t2 = new Triangle();
    Node* n1 = new Node();
    Node* n2 = new Node();
    Vertex pv = { p, primary_fg_color };

    n1->origin = p;
    n1->triangle = t1;
    n2->origin = p;
    n2->triangle = t2;
    node->children[0] = n1;
    node->children[1] = n2;
    node->children_count = 2;

    t1->v[0] = pv; t1->v[1] = t->v[right];    t1->v[2] = t->v[opposite];
    t2->v[0] = pv; t2->v[1] = t->v[opposite]; t2->v[2] = t->v[left];
    t1->neighbors[0] = t->neighbors[left]; t1->neighbors[1] = t->neighbors[right]; t1->neighbors[2] = t2;
    t2->neighbors[0] = t->neighbors[left]; t2->neighbors[1] = t1; t2->neighbors[2] = t->neighbors[opposite];
    n1->ray = t->v[right].pos;
    n2->ray = t->v[opposite].pos;

    Triangle* nt1 = t->neighbors[right];
    Triangle* nt2 = t->neighbors[opposite];
    if (nt1) {
        for (int i = 0; i < 3; i++) {
            if (nt1->neighbors[i] == t) nt1->neighbors[i] = t1;
        }
    }
    if (nt2) {
        for (int i = 0; i < 3; i++) {
            if (nt2->neighbors[i] == t) nt2->neighbors[i] = t2;
        }
    }
    node->triangle = NULL;
    delete t;

    // Now split the neighboring triangle, if any was found
    if (neighbor) {
        t = neighbor->triangle;
        Triangle* t3 = new Triangle();
        Triangle* t4 = new Triangle();
        Node* n3 = new Node();
        Node* n4 = new Node();

        if (triangle_area(t->v[0].pos, t->v[1].pos, p) == 0) {
            left = 0; right = 1; opposite = 2;
        } else if (triangle_area(t->v[1].pos, t->v[2].pos, p) == 0) {
            left = 1; right = 2; opposite = 0;
        } else if (triangle_area(t->v[2].pos, t->v[0].pos, p) == 0) {
            left = 2; right = 0; opposite = 1;
        }

        n3->origin = p;
        n3->triangle = t3;
        n4->origin = p;
        n4->triangle = t4;
        neighbor->children[0] = n3;
        neighbor->children[1] = n4;
        neighbor->children_count = 2;

        t3->v[0] = pv; t3->v[1] = t->v[right];    t3->v[2] = t->v[opposite];
        t4->v[0] = pv; t4->v[1] = t->v[opposite]; t4->v[2] = t->v[left];
        t3->neighbors[0] = t2; t3->neighbors[1] = t->neighbors[right]; t3->neighbors[2] = t4;
        t4->neighbors[0] = t1; t4->neighbors[1] = t3; t4->neighbors[2] = t->neighbors[opposite];
        n3->ray = t->v[right].pos;
        n4->ray = t->v[opposite].pos;

        // NOTE(alexander): connect first two triangles to these two
        t1->neighbors[0] = t4;
        t2->neighbors[0] = t3;

        Triangle* nt1 = t->neighbors[right];
        Triangle* nt2 = t->neighbors[opposite];
        if (nt1) {
            for (int i = 0; i < 3; i++) {
                if (nt1->neighbors[i] == t) nt1->neighbors[i] = t3;
            }
        }
        if (nt2) {
            for (int i = 0; i < 3; i++) {
                if (nt2->neighbors[i] == t) nt2->neighbors[i] = t4;
            }
        }
        neighbor->triangle = NULL;
        delete t;
    }
}

static void
split_triangle_at_point(Triangulation* result, glm::vec2 p) {
    std::vector<Node*> nodes;
    point_location(result->root, p, &nodes);
    if (nodes.size() == 0) return;
    Node* node = nodes[0];
    Node* neighbor = NULL;
    Triangle* t  = node->triangle;
    if (nodes.size() > 1) neighbor = nodes[1];
    assert(nodes.size() <= 2);
    assert(node->triangle && "not a leaf node");

    // NOTE(alexander): 3 literal edge case - splits into two triangles
    if (triangle_area(t->v[0].pos, t->v[1].pos, p) == 0) {
        split_triangle_at_edge(node, neighbor, p, 0, 1, 2);
    } else if (triangle_area(t->v[1].pos, t->v[2].pos, p) == 0) {
        split_triangle_at_edge(node, neighbor, p, 1, 2, 0);
    } else if (triangle_area(t->v[2].pos, t->v[0].pos, p) == 0) {
        split_triangle_at_edge(node, neighbor, p, 2, 0, 1);
    } else {
        // NOTE(alexander): common case - splits into three triangles
        Vertex pv = { p, primary_fg_color };
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

        t1->v[0] = t->v[0]; t1->v[1] = t->v[1], t1->v[2] = pv;
        t2->v[0] = t->v[1]; t2->v[1] = t->v[2]; t2->v[2] = pv;
        t3->v[0] = t->v[2]; t3->v[1] = t->v[0]; t3->v[2] = pv;
        t1->neighbors[0] = t->neighbors[0]; t1->neighbors[1] = t2; t1->neighbors[2] = t3;
        t2->neighbors[0] = t->neighbors[1]; t2->neighbors[1] = t3; t2->neighbors[2] = t1;
        t3->neighbors[0] = t->neighbors[2]; t3->neighbors[1] = t1; t3->neighbors[2] = t2;

        n1->ray = t->v[0].pos;
        n2->ray = t->v[1].pos;
        n3->ray = t->v[2].pos;

        node->children[2] = n3;
        node->children_count = 3;

        Triangle* nt1 = t->neighbors[0];
        Triangle* nt2 = t->neighbors[1];
        Triangle* nt3 = t->neighbors[2];
        if (nt1) {
            for (int i = 0; i < 3; i++) {
                if (nt1->neighbors[i] == t) nt1->neighbors[i] = t1;
            }
        }
        if (nt2) {
            for (int i = 0; i < 3; i++) {
                if (nt2->neighbors[i] == t) nt2->neighbors[i] = t2;
            }
        }
        if (nt3) {
            for (int i = 0; i < 3; i++) {
                if (nt3->neighbors[i] == t) nt3->neighbors[i] = t3;
            }
        }
        node->triangle = NULL;
        delete t;
    }
}

static void
build_vertex_sequence(Node* node, std::vector<Vertex>* vdata) {
    if (!node) return;
    if (node->triangle) {
        vdata->push_back(node->triangle->v[0]);
        vdata->push_back(node->triangle->v[1]);
        vdata->push_back(node->triangle->v[2]);
    }
    for (auto child : node->children) {
        build_vertex_sequence(child, vdata);
    }
}

static Triangulation
build_triangulation_of_points(const std::unordered_set<glm::vec2>& points, std::mt19937& rng) {
    std::vector<glm::vec2> convex_hull;
    build_convex_hull(points, convex_hull);
    std::unordered_set<glm::vec2> convex_hull_set(convex_hull.begin(), convex_hull.end());

    Triangulation result = {};
    Node* search_tree = build_fan_triangulation(convex_hull);
    result.root = search_tree;

    for (auto v : points) {
        if (convex_hull_set.find(v) == convex_hull_set.end()) {
            split_triangle_at_point(&result, v);
        }
    }

    build_vertex_sequence(result.root, &result.vertices);

    return result;
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
    scene->count = (GLsizei) scene->triangulation.vertices.size();

    GLvoid* data = 0;
    if (scene->count > 0) data = &scene->triangulation.vertices[0];

    glGenVertexArrays(1, &scene->vao);
    glBindVertexArray(scene->vao);
    glGenBuffers(1, &scene->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*scene->count, data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32)*6, 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(f32)*6, (GLvoid*) (sizeof(f32)*2));
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

    // Setup default settings
    scene->is_initialized = true;

    return true;
}

static void
update_scene_vbo_data(Triangulation_Scene* scene) {
    GLvoid* data = 0;
    scene->count = (GLsizei) scene->triangulation.vertices.size();
    if (scene->count > 0) data = &scene->triangulation.vertices[0];
    glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*scene->count, data, GL_STATIC_DRAW);
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
    xm /= scene->count; ym /= scene->count;

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
    update_scene_vbo_data(scene);
}

void
update_and_render_scene(Triangulation_Scene* scene, Window* window) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    // Setup orthographic projection and camera transform
    update_camera_2d(window, &scene->camera);
    glm::mat4 transform = glm::ortho(0.0f, (f32) window->width, (f32) window->height, 0.0f, 0.0f, 1000.0f);
    transform = glm::scale(transform, glm::vec3(scene->camera.zoom + 1.0f, scene->camera.zoom + 1.0f, 0));
    transform = glm::translate(transform, glm::vec3(scene->camera.x, scene->camera.y, 0));

    // Bind vertex array
    glBindVertexArray(scene->vao);

    auto retriangulate_points = [scene]() {
        destroy_triangulation(&scene->triangulation);
        scene->triangulation = build_triangulation_of_points(scene->points, scene->rng);
        calculate_distance_coloring(scene);
        update_scene_vbo_data(scene);
    };

    // Mouse interaction
    f32 x = (window->input.mouse_x/(scene->camera.zoom + 1.0f) - scene->camera.x);
    f32 y = (window->input.mouse_y/(scene->camera.zoom + 1.0f) - scene->camera.y);
    if (was_pressed(&window->input.left_mb)) {
        if (window->input.shift_key.ended_down) {
            // Push new vertex interactively
            scene->points.emplace(glm::vec2(x, y));
            retriangulate_points();

        } else if (scene->coloring_option != 1) {
            // Point location color selected node
            std::vector<Node*> nodes;
            point_location(scene->triangulation.root, glm::vec2(x, y), &nodes);
            if (nodes.size() >= 0) {
                std::vector<Vertex*> vertices;

                Node* node = nodes[0];
                Triangle* t = node->triangle;
                vertices.push_back(&t->v[0]);
                vertices.push_back(&t->v[1]);
                vertices.push_back(&t->v[2]);

                for (auto v : vertices) {
                    v->color = secondary_fg_color;
                }

                std::vector<Vertex> out_vertices;
                build_vertex_sequence(scene->triangulation.root, &out_vertices);
                scene->triangulation.vertices = out_vertices;
                update_scene_vbo_data(scene);

                for (auto v : vertices) {
                    v->color = primary_fg_color;
                }
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
    glDrawArrays(GL_TRIANGLES, 0, scene->count);

    // Render `outlined` triangulated shape
    glLineWidth(scene->camera.zoom*0.5f + 2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform4f(scene->tint_color_uniform, 0.4f, 0.4f, 0.4f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, scene->count);

    // Render `points` used in triangulated shape
    glUniform4f(scene->tint_color_uniform, 0.4f, 0.4f, 0.4f, 1.0f);
    glPointSize((scene->camera.zoom + 2.0f) + 4.0f);
    glDrawArrays(GL_POINTS, 0, scene->count);

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

    ImGui::Text("Coloring options:");
    if (ImGui::RadioButton("Default", &scene->coloring_option, 0)) {
        for (int i = 0; i < scene->triangulation.vertices.size(); i++) {
            scene->triangulation.vertices[i].color = primary_fg_color;
        }
        update_scene_vbo_data(scene);
    }

    if (ImGui::RadioButton("Coloring based on distance", &scene->coloring_option, 1)) {
        calculate_distance_coloring(scene);
    }
    
    ImGui::RadioButton("Extended picking", &scene->coloring_option, 2);
    ImGui::RadioButton("4 Coloring", &scene->coloring_option, 3);

    ImGui::Text("Info:");
    ImGui::Text("Number of points: %zu", scene->points.size());
    ImGui::Text("Number of triangles: %zu", scene->triangulation.vertices.size()/3);
    ImGui::Text("x = %.3f", x);
    ImGui::Text("y = %.3f", y);
    ImGui::End();

    // Reset states
    glLineWidth(1);
    glPointSize(1);
    glUseProgram(0);
    glBindVertexArray(0);
}
