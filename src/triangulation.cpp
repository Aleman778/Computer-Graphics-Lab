
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

struct Triangulation_Scene {
    // Main vertex buffer info containing all the points
    GLuint points_vao;
    GLsizei points_count;
    
    // Convex hull buffer info
    GLuint  convex_hull_vao;
    GLsizei convex_hull_count;

    // Resulted triangulated buffer info
    GLuint triangulation_vao;
    GLsizei triangulation_count;

    // Shader data
    GLuint shader;
    GLint  color_uniform;
    GLint  transform_uniform;

    Camera2D camera;

    bool is_initialized;
};

struct Triangle {
    glm::vec2* vertices[3];
    glm::vec4 color;
    Triangle* neighbors[3];
};

struct Node {
    glm::vec2 ray;
    Triangle* triangle; // for non-leaf nodes this is NULL.
    std::vector<Node> children;
};

struct Triangulation {
    std::vector<glm::vec2> vertices;
    Node root;
};

inline float
triangle_area(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// NOTE(alexander): Andrew's Algorithm has requirement that points are sorted in increasing x values.
std::vector<glm::vec2>
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

    points_inside = points;
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
    upper.pop_back(); // duplicate vertex
    std::reverse(upper.begin(), upper.end()); // NOTE(alexander): upper is reverse order!!!
    result.insert(result.end(), upper.begin(), upper.end());
    return points_inside;
}

Node
build_fan_triangulation(Triangulation* result,
                        const std::vector<glm::vec2>& convex_hull,
                        int beg,
                        int end) {
    Node node = {};
    glm::vec2 p0 = convex_hull[0];

    if (end - beg <= 1) { // NOTE(alexander): base case
        printf("beg = %d\tend = %d\n", beg,  end);
        result->vertices.push_back(p0);
        glm::vec2* vec = &result->vertices[result->vertices.size() - 1];
        result->vertices.push_back(convex_hull[end]);
        result->vertices.push_back(convex_hull[beg]); // NOTE(alexander): counter-clockwise ordering!

        auto tri = new Triangle();
        tri->vertices[0] = vec;
        tri->vertices[1] = vec + 1;
        tri->vertices[2] = vec + 2;
        node.triangle = tri;
        return node;
    }

    int split_index = (end - beg) / 2;
    Node left = build_fan_triangulation(result, convex_hull, beg, beg + split_index);
    Node right = build_fan_triangulation(result, convex_hull, beg + split_index, end);
    Triangle* rt = left.triangle;
    Triangle* lt = right.triangle;
    if (rt && lt) {
        lt->neighbors[0] = rt;
        rt->neighbors[2] = lt;
    }
    node.ray = convex_hull[split_index] - p0;
    node.children.push_back(left);
    node.children.push_back(right);
    result->root = node;
    return node;
}

bool
initialize_scene(Triangulation_Scene* scene) {
    // Generate random points
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<float> dist(10.0f, 710.0f);

    std::vector<glm::vec2> points;
    for (int i = 0; i < 64; i++) {
        points.push_back(glm::vec2(290.0f + dist(mt), dist(mt)));
    }

    // Setup points vertex buffer object
    auto initialize_2d_vao = [](const std::vector<glm::vec2>& data) -> GLuint {
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(glm::vec2)*data.size(),
                     &data[0].x,
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glBindVertexArray(0);
        glDeleteBuffers(1, &vbo);
        return vao;
    };

    scene->points_vao = initialize_2d_vao(points);
    scene->points_count = points.size();

    // Compute the convex hull and also points inside the hull
    std::sort(points.begin(), points.end(), [](glm::vec2 v1, glm::vec2 v2) -> bool {
        if (v1.x == v2.x) return v1.y < v2.y;
        return v1.x < v2.x;
    });

    std::vector<glm::vec2> convex_hull;
    std::vector<glm::vec2> points_inside_convex_hull;
    build_convex_hull(points, convex_hull, points_inside_convex_hull);

    // Setup convex hull vbo
    // scene->convex_hull_vao = initialize_2d_vao(convex_hull);
    // scene->convex_hull_count = convex_hull.size();


    // Compute fan triangulation of convex hull
    Triangulation result = {};
    build_fan_triangulation(&result, convex_hull, 0, convex_hull.size() - 1);
    scene->triangulation_vao = initialize_2d_vao(result.vertices);
    scene->triangulation_count = result.vertices.size();

    // for (
    // triangulate_point(&result, )

    printf("Triangulation result:\n");
    printf("\tpoints on hull = %d\n", convex_hull.size());
    printf("\tnumber of triangles = %d\n", result.vertices.size()/3);

    // Setup vertex shader
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

    // Camera 2D update
    update_camera_2d(window, &scene->camera);

    // Setup orthographic projection and camera transform
    glm::mat4 transform = glm::ortho(0.0f, (f32) window->width, (f32) window->height, 0.0f, 0.0f, 1000.0f);
    transform = glm::scale(transform, glm::vec3(scene->camera.zoom + 1.0f, scene->camera.zoom + 1.0f, 0));
    transform = glm::translate(transform, glm::vec3(scene->camera.x, scene->camera.y, 0));
    

    // Set viewport
    glViewport(0, 0, window->width, window->height);
    
    // Render and clear background
    glClearColor(1.0f, 0.99f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable shader
    glUseProgram(scene->shader);

    // Render fan triangulation
    glBindVertexArray(scene->triangulation_vao);
    glUniformMatrix4fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform4f(scene->color_uniform, 1.0f, 0.0f, 0.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, scene->triangulation_count);
    glLineWidth(3);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, scene->triangulation_count);
    
    // Render convex hull
    // glLineWidth(2);
    // glBindVertexArray(scene->convex_hull_vbo);
    // glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
    // glDrawArrays(GL_LINE_LOOP, 0, scene->convex_hull_count);

    // Render points
    glPointSize(8);
    glBindVertexArray(scene->points_vao);
    glDrawArrays(GL_POINTS, 0, scene->points_count);

    // Reset states
    glLineWidth(1);
    glPointSize(1);

    // Begin ImGUI
    if (ImGui::BeginMainMenuBar()) {
        static bool file_enabled = true;
        if (ImGui::BeginMenu("File", file_enabled)) {
            
        }

        ImGui::EndMainMenuBar();
    }
}
