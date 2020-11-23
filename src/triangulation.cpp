
struct Vertex {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Triangle {
    Vertex v[3];
    Triangle* neighbors[3];
};

struct Node {
    glm::vec2 ray; // ray is vector pointing out from origin point 
    glm::vec2 origin; // origin point
    Triangle* triangle; // for non-leaf nodes this is NULL.
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

    // Shader data
    GLuint shader;
    GLint  tint_color_uniform;
    GLint  transform_uniform;

    std::vector<glm::vec2> points;
    Triangulation triangulation;
    
    Camera2D camera;

    std::mt19937 rng;

    bool show_gui;
    
    bool is_initialized;
};

inline float
triangle_area(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// NOTE(alexander): Andrew's Algorithm has requirement that points are sorted in increasing x values.
void
build_convex_hull(const std::vector<glm::vec2>& points,
                  std::vector<glm::vec2>& result,
                  std::vector<glm::vec2>& points_inside) {
    auto compute_half_convex_hull =
        [points](std::vector<glm::vec2>& points_inside, bool lower) -> std::vector<glm::vec2> {
        std::vector<glm::vec2> result;
        for (int i = 0; i < points.size(); i++) {
            glm::vec2 q = points[i]; // query point
            while (result.size() >= 2) {
                glm::vec2 a = result[result.size() - 2];
                glm::vec2 b = result[result.size() - 1];

                float area = triangle_area(a, b, q);
                if (!lower) area *= -1.0; // NOTE(alexander): upper hull invert the area

                // If the determinant is positive means CCW turn
                if (area > 0) {
                    if (lower) points_inside.push_back(result.back());
                    result.pop_back();
                } else {
                    break;
                }
            }
            result.push_back(q);
        }
        return result;
    };

    result = compute_half_convex_hull(points_inside, true);
    std::vector<glm::vec2> upper = compute_half_convex_hull(points_inside, false);

    // HACK(alexander): remove points inside lower hull that are on the upper hull
    for (auto q : upper) {
        for (int i = 0; i < points_inside.size(); i++) {
            auto v = points_inside[i];
            if (v == q) {
                points_inside.erase(points_inside.begin() + i);
                break;
            }
        }
    }

    if (upper.size() > 0) upper.pop_back();   // last is duplicate vertex
    std::reverse(upper.begin(), upper.end()); // NOTE(alexander): upper is reverse order!!!
    if (upper.size() > 0) upper.pop_back();   // first is also duplicate vertex
    result.insert(result.end(), upper.begin(), upper.end());
}

Node*
build_fan_triangulation(const std::vector<glm::vec2>& convex_hull, int beg, int end) {
    Node* node = new Node();
    if (end - beg <= 0) return node;
    glm::vec2 p0 = convex_hull[0];

    if (end - beg == 1) { // NOTE(alexander): base case
        Triangle* t = new Triangle();
        t->v[0] = { p0, primary_fg_color };
        t->v[1] = { convex_hull[beg], primary_fg_color };
        t->v[2] = { convex_hull[end], primary_fg_color };
        node->triangle = t;
        node->origin = p0;
        node->ray = convex_hull[beg];
        node->children_count = 0;
        return node;
    }

    int split_index = (end - beg)/2;
    Node* left = build_fan_triangulation(convex_hull, beg, beg + split_index);
    Node* right = build_fan_triangulation(convex_hull, beg + split_index, end);
    Triangle* rt = left->triangle;
    Triangle* lt = right->triangle;
    if (rt && lt) {
        lt->neighbors[0] = rt;
        rt->neighbors[2] = lt;
    }
    node->origin = p0;
    node->ray = convex_hull[beg];
    node->children[0] = left;
    node->children[1] = right;
    node->children_count = 2;
    return node;
}

void
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

void
split_triangle_at_point(Triangulation* result, glm::vec2 p) {
    std::vector<Node*> nodes;
    point_location(result->root, p, &nodes);

    for (auto node : nodes) {
        assert(node->triangle && "not a leaf node");
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
        
        // NOTE(alexander): 3 literal edge case - splits into two triangles
        if (triangle_area(t->v[0].pos, t->v[1].pos, p) == 0) {
            t1->v[0] = pv; t1->v[1] = t->v[1]; t1->v[2] = t->v[2];
            t2->v[0] = pv; t2->v[1] = t->v[2]; t2->v[2] = t->v[0];
            t1->neighbors[0] = t->neighbors[0]; t1->neighbors[1] = t->neighbors[1]; t1->neighbors[2] = t2;
            t2->neighbors[0] = t->neighbors[0]; t2->neighbors[1] = t1; t2->neighbors[2] = t->neighbors[2];
            n1->ray = t->v[1].pos;
            n2->ray = t->v[2].pos;

        } else if (triangle_area(t->v[1].pos, t->v[2].pos, p) == 0) {
            t1->v[0] = pv; t1->v[1] = t->v[2]; t1->v[2] = t->v[0];
            t2->v[0] = pv; t2->v[1] = t->v[0]; t2->v[2] = t->v[1];
            t1->neighbors[0] = t->neighbors[1]; t1->neighbors[1] = t->neighbors[2]; t1->neighbors[2] = t2;
            t2->neighbors[0] = t->neighbors[1]; t2->neighbors[1] = t1; t2->neighbors[2] = t->neighbors[0];
            n1->ray = t->v[2].pos;
            n2->ray = t->v[0].pos;

        } else if (triangle_area(t->v[2].pos, t->v[0].pos, p) == 0) {
            t1->v[0] = pv; t1->v[1] = t->v[0]; t1->v[2] = t->v[1];
            t2->v[0] = pv; t2->v[1] = t->v[1], t2->v[2] = t->v[2];
            t1->neighbors[0] = t->neighbors[2]; t1->neighbors[1] = t->neighbors[0]; t1->neighbors[2] = t2;
            t2->neighbors[0] = t->neighbors[2]; t2->neighbors[1] = t1; t2->neighbors[2] = t->neighbors[1];
            n1->ray = t->v[0].pos;
            n2->ray = t->v[1].pos;

        } else { // NOTE(alexander): common case - splits into three triangles
            Node* n3 = new Node();
            Triangle* t3 = new Triangle();
            n3->origin = p;
            n3->triangle = t3;

            t1->v[0] = t->v[0]; t1->v[1] = t->v[1], t1->v[2] = pv;
            t2->v[0] = t->v[1]; t2->v[1] = t->v[2]; t2->v[2] = pv;
            t3->v[0] = t->v[2]; t3->v[1] = t->v[0]; t3->v[2] = pv;
            t1->neighbors[0] = t->neighbors[0]; t1->neighbors[1] = t3; t1->neighbors[2] = t2;
            t2->neighbors[0] = t->neighbors[1]; t2->neighbors[1] = t1; t2->neighbors[2] = t3;
            t3->neighbors[0] = t->neighbors[2]; t3->neighbors[1] = t1; t3->neighbors[2] = t2;
            
            n1->ray = t->v[0].pos;
            n2->ray = t->v[1].pos;
            n3->ray = t->v[2].pos;
            
            node->children[2] = n3;
            node->children_count = 3;

        }

        node->triangle = NULL;
        delete t;
    }
}

void
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

Triangulation
build_triangulation_of_points(const std::vector<glm::vec2>& points, std::mt19937& rng) {
    // Compute the convex hull and also points inside the hull
    std::vector<glm::vec2> sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(), [](glm::vec2 v1, glm::vec2 v2) -> bool {
        if (v1.x == v2.x) return v1.y < v2.y;
        return v1.x < v2.x;
    });

    std::vector<glm::vec2> convex_hull;
    std::vector<glm::vec2> points_inside_convex_hull;
    build_convex_hull(sorted_points, convex_hull, points_inside_convex_hull);

    // Shuffle the order of points inside convex hull, avoid adversarial input
    std::shuffle(points_inside_convex_hull.begin(), points_inside_convex_hull.end(), rng);

    printf("points: %zu\n", points.size());
    printf("points on the hull: %zu\n", convex_hull.size());
    printf("points inside convex hull: %zu\n\n", points_inside_convex_hull.size());
    
    // Compute fan triangulation of convex hull
    Triangulation result = {};
    Node* search_tree = build_fan_triangulation(convex_hull, 0, (int) convex_hull.size() - 1);
    result.root = search_tree;

    for (auto v : points_inside_convex_hull) {
        split_triangle_at_point(&result, v);
    }

    build_vertex_sequence(result.root, &result.vertices);
    
    return result;
}

void
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

bool
initialize_scene(Triangulation_Scene* scene) {
    // Generate random points
    std::random_device rd;
    scene->rng = std::mt19937(rd());
    std::uniform_real_distribution<float> dist(10.0f, 710.0f);

    for (int i = 0; i < 64; i++) {
        scene->points.push_back(glm::vec2(290.0f + dist(scene->rng), dist(scene->rng)));
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*6, 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float)*6, (GLvoid*) (sizeof(float)*2));
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
        scene->count = (GLsizei) scene->triangulation.vertices.size();
        GLvoid* data = 0;
        if (scene->count > 0) data = &scene->triangulation.vertices[0];
        glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*scene->count, data, GL_STATIC_DRAW);
    };

    // Mouse interaction
    f32 x = (window->input.mouse_x/(scene->camera.zoom + 1.0f) - scene->camera.x);
    f32 y = (window->input.mouse_y/(scene->camera.zoom + 1.0f) - scene->camera.y);
    if (was_pressed(&window->input.left_mb)) {
        if (window->input.shift_key.ended_down) {
            // Push new vertex interactively
            scene->points.push_back(glm::vec2(x, y));
            retriangulate_points();
            
        } else {
            // Point location color selected node
            std::vector<Node*> nodes;
            point_location(scene->triangulation.root, glm::vec2(x, y), &nodes);
            for (auto node : nodes) {
                if (!node->triangle) continue;
                node->triangle->v[0].color = secondary_fg_color;
                node->triangle->v[1].color = secondary_fg_color;
                node->triangle->v[2].color = secondary_fg_color;
            }

            std::vector<Vertex> vertices;
            build_vertex_sequence(scene->triangulation.root, &vertices);
            GLvoid* data = 0;
            if (scene->count > 0) data = &vertices[0];
            glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*scene->count, data, GL_STATIC_DRAW);

            for (auto node : nodes) {
                if (!node->triangle) continue;
                node->triangle->v[0].color = primary_fg_color;
                node->triangle->v[1].color = primary_fg_color;
                node->triangle->v[2].color = primary_fg_color;
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
    glUniform4f(scene->tint_color_uniform, 0.2f, 0.2f, 0.2f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, scene->count);

    // Render `points` used in triangulated shape
    glUniform4f(scene->tint_color_uniform, 0.2f, 0.2f, 0.2f, 1.0f);
    glPointSize((scene->camera.zoom + 2.0f) + 4.0f);
    glDrawArrays(GL_POINTS, 0, scene->count);

    // Reset states
    glLineWidth(1);
    glPointSize(1);
    glUseProgram(0);
    glBindVertexArray(0);

    // Begin ImGui
    ImGui::Begin("Triangulation", &scene->show_gui, ImVec2(200, 300), ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Button("Clear Points")) {
        scene->points.clear();
        retriangulate_points();
    }
    if (ImGui::Button("Fixed Points")) {
        scene->points.clear();
        scene->points.push_back(glm::vec2(200,  400));
        scene->points.push_back(glm::vec2(1000, 400));
        scene->points.push_back(glm::vec2(600,  200));
        scene->points.push_back(glm::vec2(800,  200));
        scene->points.push_back(glm::vec2(600,  600));
        scene->points.push_back(glm::vec2(600,  400));
        retriangulate_points();
    }
    if (ImGui::Button("Randomize Points")) {
        scene->points.clear();
        std::uniform_real_distribution<float> dist(10.0f, 710.0f);
        for (int i = 0; i < 64; i++) {
            scene->points.push_back(glm::vec2(290.0f + dist(scene->rng), dist(scene->rng)));
        }
        retriangulate_points();
    }
    if (ImGui::Button("Remove Last Point") && scene->points.size() > 0) {
        scene->points.pop_back();
        retriangulate_points();
    }
    ImGui::Text("Number of points: %zu", scene->points.size());
    ImGui::Text("Number of triangles: %zu", scene->triangulation.vertices.size()/3);
    ImGui::Text("x = %.3f", x);
    ImGui::Text("y = %.3f", y);
    ImGui::End();
}
