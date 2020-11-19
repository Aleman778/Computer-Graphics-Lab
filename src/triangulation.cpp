
const char* vert_shader_source =
    "#version 330\n"
    "layout(location=0) in vec2 pos;\n"
    "uniform mat4 transform;\n"
    "void main() {\n"
    "    gl_Position = transform * vec4(pos, 0.0f, 1.0f);\n"
    "}\n";

const char* frag_shader_source =
    "#version 330\n"
    "out vec4 frag_color;\n"
    "uniform vec4 color;\n"
    "void main() {\n"
    "    frag_color = color;\n"
    "}\n";

struct Triangulation {
    std::vector<glm::vec2> vertices;
};

struct Triangulation_Scene {
    Mesh point_mesh;
    Mesh triangulated_mesh; // Triangulated mesh

    // Shader data
    GLuint shader;
    GLint  color_uniform;
    GLint  transform_uniform;

    std::vector<glm::vec2> points;
    Triangulation triangulation;
    
    Camera2D camera;

    std::mt19937 rng;

    bool is_initialized;
};

struct Triangle {
    glm::vec2 vertices[3];
    glm::vec4 color;
    Triangle* neighbors[3];
};

struct Node {
    glm::vec2 ray; // ray is vector pointing out from origin point 
    glm::vec2 origin; // origin point
    Triangle* triangle; // for non-leaf nodes this is NULL.
    std::vector<Node*> children;
};

inline float
triangle_area(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

inline void
push_back_triangle(std::vector<glm::vec2>& vertices,
                   glm::vec2 vlist[],
                   glm::vec2 v0,
                   glm::vec2 v1,
                   glm::vec2 v2) {
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vlist[0] = v0;
    vlist[1] = v1;
    vlist[2] = v2;
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
                if (lower) area *= -1.0; // NOTE(alexander): upper hull invert the area

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
    for (int i = 0; i < points_inside.size(); i++) {
        for (auto q : upper) {
            auto v = points_inside[i];
            if (v == q) {
                points_inside.erase(points_inside.begin() + i);
                break;
            }
        }
    }
    upper.pop_back(); // last is duplicate vertex
    std::reverse(upper.begin(), upper.end()); // NOTE(alexander): upper is reverse order!!!
    upper.pop_back(); // first is also duplicate vertex
    result.insert(result.end(), upper.begin(), upper.end());
}

Node*
build_fan_triangulation(Triangulation* result,
                        const std::vector<glm::vec2>& convex_hull,
                        int beg,
                        int end) {
    Node* node = new Node();
    glm::vec2 p0 = convex_hull[0];

    if (end - beg <= 1) { // NOTE(alexander): base case
        Triangle* tri = new Triangle();
        push_back_triangle(result->vertices, tri->vertices, p0, convex_hull[end], convex_hull[beg]);
        node->triangle = tri;
        node->origin = p0;
        node->ray = convex_hull[beg];
        return node;
    }

    int split_index = (end - beg) / 2;
    Node* left = build_fan_triangulation(result, convex_hull, beg, beg + split_index);
    Node* right = build_fan_triangulation(result, convex_hull, beg + split_index, end);
    Triangle* rt = left->triangle;
    Triangle* lt = right->triangle;
    if (rt && lt) {
        lt->neighbors[0] = rt;
        rt->neighbors[2] = lt;
    }
    node->origin = p0;
    node->ray = convex_hull[beg];
    node->children.push_back(left);
    node->children.push_back(right);
    return node;
}

Node*
point_location(Node* node, glm::vec2 p) {
    if (node->triangle) return node;

    // NOTE(alexander): pick the node with smallest POSITIVE area.
    f32 smallest_area = 10000000.0f;
    Node* next_node = NULL;
    for (auto child : node->children) {
        f32 area = triangle_area(child->origin, child->ray, p);
        printf("area: %f\n", area);
        if (area >= 0 && area < smallest_area) {
            smallest_area = area;
            next_node = child;
        }
    }
    if (!next_node) return node;
    return point_location(next_node, p);
} 

void
split_triangle_at_point(Triangulation* result, Node* node, glm::vec2 p) {
    assert(node->triangle && "not a leaf node");

    // NOTE(alexander): split triangle into three peices, common case!
    Triangle* t  = node->triangle;
    Triangle* t1 = new Triangle();
    Triangle* t2 = new Triangle();
    Triangle* t3 = new Triangle();
    push_back_triangle(result->vertices, t1->vertices, t->vertices[0], t->vertices[1], p);
    push_back_triangle(result->vertices, t2->vertices, t->vertices[1], t->vertices[2], p);
    push_back_triangle(result->vertices, t3->vertices, t->vertices[2], t->vertices[0], p);
    t1->neighbors[0] = t->neighbors[0]; t1->neighbors[1] = t3; t1->neighbors[2] = t2;
    t2->neighbors[0] = t->neighbors[1]; t2->neighbors[1] = t1; t2->neighbors[2] = t3;
    t3->neighbors[0] = t->neighbors[2]; t3->neighbors[1] = t1; t3->neighbors[2] = t2;
    Node* n1 = new Node();
    Node* n2 = new Node();
    Node* n3 = new Node();
    n1->origin = p;
    n1->ray = t->vertices[1];
    n1->triangle = t1;
    n2->origin = p;    
    n2->ray = t->vertices[2];
    n2->triangle = t2;
    n3->origin = p;
    n3->ray = t->vertices[0];
    n3->triangle = t3;
    node->children.push_back(n1);
    node->children.push_back(n2);
    node->children.push_back(n3);
    node->triangle = NULL;
}

Triangulation
triangulate_points(const std::vector<glm::vec2>& points, std::mt19937& rng) {
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
    
    // Compute fan triangulation of convex hull
    Triangulation result = {};
    Node* search_tree = build_fan_triangulation(&result, convex_hull, 0, (int) convex_hull.size() - 1);
    printf("points inside hull: %zu\n", points_inside_convex_hull.size());
    for (auto v : points_inside_convex_hull) {
        Node* leaf_node = point_location(search_tree, v);
        if (leaf_node->triangle) {
            split_triangle_at_point(&result, leaf_node, v);
        }
    }
    return result;
}

bool
initialize_scene(Triangulation_Scene* scene) {
    // Generate random points
    std::random_device rd;
    scene->rng = std::mt19937(rd());
    std::uniform_real_distribution<float> dist(10.0f, 710.0f);

    for (int i = 0; i < 10; i++) {
        scene->points.push_back(glm::vec2(290.0f + dist(scene->rng), dist(scene->rng)));
    }

    scene->point_mesh = create_2d_mesh(scene->points);

    scene->triangulation = triangulate_points(scene->points, scene->rng);
    scene->triangulated_mesh = create_2d_mesh(scene->triangulation.vertices);

    scene->shader = load_glsl_shader_from_sources(vert_shader_source, frag_shader_source);
    scene->color_uniform = glGetUniformLocation(scene->shader, "color");
    if (scene->color_uniform == -1) {
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

    // Push new vertex interactively
    if (was_pressed(&window->input.left_mb)) {
        f32 x = (window->input.mouse_x/(scene->camera.zoom + 1.0f) - scene->camera.x);
        f32 y = (window->input.mouse_y/(scene->camera.zoom + 1.0f) - scene->camera.y);

        scene->points.push_back(glm::vec2(x, y));
        scene->triangulation = triangulate_points(scene->points, scene->rng);

        update_2d_mesh(&scene->point_mesh, scene->points);
        update_2d_mesh(&scene->triangulated_mesh, scene->triangulation.vertices);
    }

    // Set viewport
    glViewport(0, 0, window->width, window->height);

    // Render and clear background
    glClearColor(1.0f, 0.99f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable shader
    glUseProgram(scene->shader);

    // Render triangulated mesh
    glBindVertexArray(scene->triangulated_mesh.vao);
    glUniformMatrix4fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform4f(scene->color_uniform, 1.0f, 0.0f, 0.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, scene->triangulated_mesh.count);
    glLineWidth(3);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, scene->triangulated_mesh.count);

    // Render points
    glPointSize(2.0f * (scene->camera.zoom + 2.0f) + 4.0f);
    glBindVertexArray(scene->point_mesh.vao);
    glDrawArrays(GL_POINTS, 0, scene->point_mesh.count);

    // Reset states
    glLineWidth(1);
    glPointSize(1);

    // Begin ImGUI
    // if (ImGui::BeginMainMenuBar()) {
        // static bool file_enabled = true;
        // if (ImGui::BeginMenu("File", file_enabled)) {
    // 
        // }

        // ImGui::EndMainMenuBar();
    // }
}
