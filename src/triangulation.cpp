
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
    GLuint triangulated_vao;
    GLsizei triangulated_count;

    // Shader data
    GLuint shader;
    GLint  color_uniform;
    GLint  transform_uniform;

    
    glm::mat4 ortho_projection;

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
    Node* children[3]; // for binary split nodes[2] is NULL.
};

struct Triangulation {
    std::vector<glm::vec2> vertices;
    Node root;
};

void
scene_window_size_callback(void* scene, i32 width, i32 height) {
    ((Triangulation_Scene*) scene)->ortho_projection = glm::ortho(0.0f, (f32) width, (f32) height, 0.0f, 0.0f, 1000.0f);
    glViewport(0, 0, width, height);
}

inline float
triangle_area(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// NOTE(alexander): andrew's algorithm prerequsite is that input must be sorted with increasing x value
std::vector<glm::vec2>
build_convex_hull(const std::vector<glm::vec2>& input) {
    auto compute_half_convex_hull = [input](bool lower) -> std::vector<glm::vec2> {
        std::vector<glm::vec2> result;
        for (int i = 0; i < input.size(); i++) {
            glm::vec2 q = input[i]; // query point
            while (result.size() >= 2) {
                glm::vec2 a = result[result.size() - 2];
                glm::vec2 b = result[result.size() - 1];

                float area = triangle_area(a, b, q);
                if (lower) area *= -1.0; // NOTE(alexander): upper hull invert the area

                // If the determinant is positive means CCW turn
                if (area > 0) {
                    result.pop_back();
                } else {
                    break;
                }
            }
            result.push_back(q);
        }
        return result;
    };

    std::vector<glm::vec2> lower = compute_half_convex_hull(true);
    std::vector<glm::vec2> upper = compute_half_convex_hull(false);
    upper.pop_back(); // duplicate vertex
    std::reverse(upper.begin(), upper.end()); // NOTE(alexander): upper is reverse order!!!
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

Triangle
build_fan_triangulation(Triangulation* result,
                        Node* node,
                        const std::vector<glm::vec2>& convex_hull,
                        int beg,
                        int end) {
    glm::vec2 p0 = convex_hull[0];

    if (end - beg <= 1) {
        glm::vec2* vec = &result->vertices[result->vertices->size() - 1];
        result->vertices.push_back(p0);
        result->vertices.push_back(convex_hull[end]);
        result->vertices.push_back(convex_hull[beg]); // NOTE(alexander): counter-clockwise ordering!
        
        Triangle tri = {};
        tri.vertices[0] = vec;
        tri.vertices[1] = vec + 1;
        tri.vertices[2] = vec + 2;
        node->triangle = &tri;
        return tri;
    }

    
    int split_index = (end - beg) / 2; // NOTE(alexander): maybe not the best choice of split index!
    node.ray = convex_hull[split_index] - p0;
    Triangle t1 = build_fan_triangulation(result, &node->children[0], convex_hull, 0, split_index);
    Triangle t2 = build_fan_triangulation(result, &node->children[1], convex_hull, split_index + 1, end);
    t1.neighbors[0] = &t2;
    t2.neighbors[2] = &t1;
}
    
bool
initialize_scene(Triangulation_Scene* scene) {
    // Generate trandom points
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
        glBindVertexArrays(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(glm::vec2)*data.size(),
                     &data[0].x,
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDeleteBuffer(vbo);
        return vao;
    };

    scene->points_vao = initialize_2d_vao(points);
    scene->points_count = points.size();
    // The input points are first storted
    std::sort(points.begin(), points.end(), [](glm::vec2 v1, glm::vec2 v2) -> bool {
        if (v1.x == v2.x) return v1.y < v2.y;
        return v1.x < v2.x;
    });

    // Compute the convex hull
    std::vector<glm::vec2> convex_hull = build_convex_hull(points);

    // Setup convex hull vbo
    scene->convex_hull_vao = initialize_2d_vao(convex_hull);
    scene->convex_hull_count = convex_hull.size();


    // Compute fan triangulation of convex hull
    Triangulation result = {};
    build_fan_triangulation(&result, &result.root, convex_hull);
    scene->triangulation_vao = initialize_2d_vao(result.vertices);
    scene->triangulation_count = fan_triangulation.indices.size();

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
update_and_render_scene(Triangulation_Scene* scene) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    // Render and clear background
    glClearColor(1.0f, 0.99f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable shader
    glUseProgram(scene->shader);

    // Render fan triangulation
    glBindBuffer(GL_ARRAY_BUFFER, scene->fan_triangulation_vbo);
    glUniformMatrix4fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(scene->ortho_projection));
    glUniform4f(scene->color_uniform, 1.0f, 0.0f, 0.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, scene->fan_triangulation_count);
    glLineWidth(3);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, scene->fan_triangulation_count);
    
    // Render convex hull
    // glLineWidth(2);
    // glBindBuffer(GL_ARRAY_BUFFER, scene->convex_hull_vbo);
    // glEnableVertexAttribArray(0);
    // glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    // glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
    // glDrawArrays(GL_LINE_LOOP, 0, scene->convex_hull_count);

    // Render points
    glPointSize(8);
    glBindBuffer(GL_ARRAY_BUFFER, scene->points_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
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
