
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
    // Original points buffer info
    GLuint  points_vbo;
    GLsizei points_count;
    
    // Convex hull buffer info
    GLuint  convex_hull_vbo;
    GLsizei convex_hull_count;

    // Convex hull buffer info
    GLuint  fan_triangulation_vbo;
    GLsizei fan_triangulation_count;

    // Resulted triangulated buffer info
    GLuint vbo;
    GLuint count;

    // Shader data
    GLuint  shader;
    GLint   color_uniform;
    GLint   transform_uniform;

    
    glm::mat4 ortho_projection;

    bool is_initialized;
};

struct Triangulation {
    std::vector<glm::vec2> vertices;
    std::vector<int> triangles; // index into vertices (first vertex in a neighboring triangle)
};

void
scene_window_size_callback(void* scene, i32 width, i32 height) {
    ((Triangulation_Scene*) scene)->ortho_projection = glm::ortho(0.0f, (f32) width, (f32) height, 0.0f, 0.0f, 1000.0f);
    glViewport(0, 0, width, height);
}

// NOTE(alexander): andrew's algorithm prerequsite is that input must be sorted with increasing x value
std::vector<glm::vec2>
compute_convex_hull(const std::vector<glm::vec2>& input) {
    auto compute_half_convex_hull = [input](bool upper) -> std::vector<glm::vec2> {
        std::vector<glm::vec2> result;
        for (int i = 0; i < input.size(); i++) {
            glm::vec2 q = input[i]; // query point
            while (result.size() >= 2) {
                glm::vec2 a = result[result.size() - 2];
                glm::vec2 b = result[result.size() - 1];

                // Compute the determinant
                glm::vec2 ab = a - b;
                glm::vec2 qa = q - a;
                float det = ab.x * qa.y - qa.x * ab.y;
                if (upper) det *= -1.0;

                // If the determinant is positive means CCW turn
                if (det > 0) {
                    result.pop_back();
                } else {
                    break;
                }
            }
            result.push_back(q);
        }
        return result;
    };

    std::vector<glm::vec2> lower = compute_half_convex_hull(false);
    std::vector<glm::vec2> upper = compute_half_convex_hull(true);
    upper.pop_back(); // duplicate vertex
    std::reverse(upper.begin(), upper.end()); // NOTE(alexander): upper is reverse order!!!
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

Triangulation
compute_fan_triangulation(const std::vector<glm::vec2>& input) {
    Triangulation result = {};
    glm::vec2 p = input[0];
    for (int i = 1; i < input.size() - 1; i++) {
        result.vertices.push_back(p);
        result.vertices.push_back(input[i]);
        result.vertices.push_back(input[i + 1]);
        result.triangles.push_back(-1); // NOTE(alexander): always one edge that has no neighbor
        result.triangles.push_back(i*3 - 1);
        result.triangles.push_back((i == input.size() - 1) ? -1 : i*3 + 1);
    }
    return result;
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

    // Setup points vbo
    glGenBuffers(1, &scene->points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene->points_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*points.size(),
                 &points[0].x,
                 GL_STATIC_DRAW);
    scene->points_count = points.size();

    // Compute convex hull
    std::sort(points.begin(), points.end(), [](glm::vec2 v1, glm::vec2 v2) -> bool {
        if (v1.x == v2.x) return v1.y < v2.y;
        return v1.x < v2.x;
    });
    printf("first = %f\n, last = %f\n", points[0].x, points[35].x);
    std::vector<glm::vec2> convex_hull = compute_convex_hull(points);

    // Setup convex hull vbo
    glGenBuffers(1, &scene->convex_hull_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene->convex_hull_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*convex_hull.size(),
                 &convex_hull[0].x,
                 GL_STATIC_DRAW);
    scene->convex_hull_count = convex_hull.size();


    // Compute fan triangulation of convex hull
    Triangulation fan_triangulation = compute_fan_triangulation(convex_hull);
    glGenBuffers(1, &scene->fan_triangulation_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene->fan_triangulation_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*fan_triangulation.vertices.size(),
                 &fan_triangulation.vertices[0].x,
                 GL_STATIC_DRAW);
    scene->fan_triangulation_count = fan_triangulation.vertices.size();

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
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
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
}

