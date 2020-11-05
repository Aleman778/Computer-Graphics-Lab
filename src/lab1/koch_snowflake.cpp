

/***************************************************************************
 * Lab assignment 1 - Alexander Mennborg
 * Koch Snowflake - https://en.wikipedia.org/wiki/Koch_snowflake
 ***************************************************************************/

const int MAX_RECURSION_DEPTH = 7;

const char* vert_shader_source =
    "#version 330\n"
    "layout(location=0) in vec2 pos;\n"
    "uniform mat3 transform;\n"
    "void main() {\n"
    "    gl_Position = vec4(transform*vec3(pos, 1.0f), 1.0f);\n"
    "}\n";

const char* frag_shader_source =
    "#version 330\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
    "}\n";

struct Koch_Snowflake_Scene {
    GLuint  vertex_buffers[MAX_RECURSION_DEPTH];
    GLsizei vertex_count[MAX_RECURSION_DEPTH];
    GLuint  shader;
    GLint   transform_uniform;

    float translation;
    float rotation;
    float scale;

    int recursion_depth;
    bool enable_translation;
    bool enable_rotation;
    bool enable_scale;
    bool show_gui;
    bool is_initialized;
};

void
gen_koch_showflake_buffers(Koch_Snowflake_Scene* scene,
                           std::vector<glm::vec2>* in_vertices,
                           int depth) {
    assert(depth > 0);
    if (depth > 7) {
        return;
    }

    size_t size = in_vertices ? in_vertices->size()*4 : 3;
    std::vector<glm::vec2> vertices(size);
    if (depth == 1) {
        glm::vec2 p1( 0.0f,  0.85f);
        glm::vec2 p2( 0.7f, -0.35f);
        glm::vec2 p3(-0.7f, -0.35f);
        vertices[0] = p1;
        vertices[1] = p2;
        vertices[2] = p3;
    } else {
        assert(in_vertices->size() >= 3 && "in_vertices must have atleast three vertices");
        for (int i = 0; i < in_vertices->size(); i++) {
            glm::vec2 p0 = (*in_vertices)[i];
            glm::vec2 p1;
            if (i < in_vertices->size() - 1) {
                p1 = (*in_vertices)[i + 1];
            } else {
                p1 = (*in_vertices)[0];
            }
            glm::vec2 d = p1 - p0;
            glm::vec2 n(-d.y, d.x);
            n = glm::normalize(n);
            glm::vec2 q0 = p0 + d/3.0f;
            glm::vec2 m  = p0 + d/2.0f;
            glm::vec2 q1 = q0 + d/3.0f;
            glm::vec2 a = m + n*glm::length(d)*glm::sqrt(3.0f)/6.0f; // height of equilateral triangle
            
            vertices[i*4]     = p0;
            vertices[i*4 + 1] = q0;
            vertices[i*4 + 2] = a;
            vertices[i*4 + 3] = q1;
        }
    }

    glGenBuffers(1, &scene->vertex_buffers[depth - 1]);
    glBindBuffer(GL_ARRAY_BUFFER, scene->vertex_buffers[depth - 1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*vertices.size(),
                 &vertices[0].x,
                 GL_STATIC_DRAW);
    scene->vertex_count[depth - 1] = (GLsizei) vertices.size();
    gen_koch_showflake_buffers(scene, &vertices, depth + 1);
    return;
}

bool
initialize_scene(Koch_Snowflake_Scene* scene) {
    // Setup vertex buffers for each snowflake
    gen_koch_showflake_buffers(scene, NULL, 1);

    // Setup vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLint length = (GLint) std::strlen(vert_shader_source);
    glShaderSource(vs, 1, &vert_shader_source, &length);
    glCompileShader(vs);
    GLint log_length;
    glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        GLchar* buf = new GLchar[log_length];
        glGetShaderInfoLog(vs, log_length, NULL, buf);
        printf("[OpenGL] Vertex shader compilation error: %s", buf);
        delete[] buf;
        return false;
    }
    

    // Setup fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    length = (GLint) std::strlen(frag_shader_source);
    glShaderSource(fs, 1, &frag_shader_source, &length);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        GLchar* buf = new GLchar[log_length];
        glGetShaderInfoLog(fs, log_length, NULL, buf);
        printf("[OpenGL] Fragment shader compilation error: %s", buf);
        delete[] buf;
        return false;
    }

    // Create shader program
    scene->shader = glCreateProgram();
    glAttachShader(scene->shader, vs);
    glAttachShader(scene->shader, fs);
    glLinkProgram(scene->shader);
    glGetProgramiv(scene->shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        GLchar* buf = new GLchar[log_length];
        glGetProgramInfoLog(scene->shader, log_length, NULL, buf);
        printf("[OpenGL] Program link error: %s", buf);
        delete[] buf;
        return false;
    }

    // Delete vertex and fragment shaders, no longer needed
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Setup shader uniform location
    scene->transform_uniform = glGetUniformLocation(scene->shader, "transform");
    if (scene->transform_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `transform`");
        return false;
    }

    // Setup default settings
    scene->translation        = 0.0f;
    scene->rotation           = 0.0f;
    scene->scale              = 0.0f;
    scene->recursion_depth    = 1;
    scene->show_gui           = false;
    scene->enable_translation = false;
    scene->enable_rotation    = false;
    scene->enable_scale       = false;
    scene->is_initialized     = true;

    return true;
}

void
window_size_callback(GLFWwindow* win, i32 width, i32 height) {
    i32 size = width < height ? width : height;
    i32 x = width/2 - size/2;
    i32 y = height/2 - size/2;
    glViewport(x, y, size, size);
}

void
update_and_render_scene(Koch_Snowflake_Scene* scene) {
    if (!scene->is_initialized) {
        if (!initialize_scene(scene)) {
            is_running = false;
            return;
        }
    }

    float scale = cos(scene->scale) * 0.4f + 0.6f;
    if (scene->enable_scale) {
        scene->scale += 0.02f;
    }

    if (scene->enable_rotation) {
        scene->rotation += 0.02f;
    }

    glm::vec2 pos(0.0f, 0.0f);
    if (scene->enable_translation) {
        pos.x = 0.3f*cos(scene->translation);
        pos.y = 0.3f*sin(scene->translation);
        scene->translation += 0.02f;
    }

    glm::mat3 transform(1.0f);
    transform = glm::translate(transform, pos);
    transform = glm::scale(transform, glm::vec2(scale, scale));
    transform = glm::rotate(transform, scene->rotation);


    // Render and clear background
    glClearColor(1.0f, 0.99f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render the koch snowflake
    glBindBuffer(GL_ARRAY_BUFFER, scene->vertex_buffers[scene->recursion_depth - 1]);
    glUseProgram(scene->shader);
    glUniformMatrix3fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(transform));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glLineWidth(2);
    glDrawArrays(GL_LINE_LOOP, 0, scene->vertex_count[scene->recursion_depth - 1]);
    glLineWidth(1);

    // Draw GUI
    ImGui::Begin("Koch Snowflake", &scene->show_gui, ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Recursion Depth");
    ImGui::SliderInt("", &scene->recursion_depth, 1, MAX_RECURSION_DEPTH);
    ImGui::Text("Vertex Count: %zd", scene->vertex_count[scene->recursion_depth - 1]);
    ImGui::Checkbox("Enable translation: ", &scene->enable_translation);
    ImGui::Checkbox("Enable rotation: ", &scene->enable_rotation);
    ImGui::Checkbox("Enable scaling: ", &scene->enable_scale);
    ImGui::End();
}
