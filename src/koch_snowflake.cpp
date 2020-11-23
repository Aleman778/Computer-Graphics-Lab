
/***************************************************************************
 * Lab assignment 1 - Alexander Mennborg
 * Koch Snowflake - https://en.wikipedia.org/wiki/Koch_snowflake
 ***************************************************************************/

const int max_recursion_depth = 7;

struct Koch_Snowflake_Scene {
    // Fill vertex buffer info
    GLuint  fill_vbo[max_recursion_depth];
    GLsizei fill_count[max_recursion_depth];

    // Outline vertex buffer info
    GLuint  outline_vbo[max_recursion_depth];
    GLsizei outline_count[max_recursion_depth];
    
    // Shader data
    GLuint  shader;
    GLint   transform_uniform;
    GLint   color_uniform;

    // Transformation data
    float translation;
    float rotation;
    float scale;

    // Render state
    int recursion_depth;
    bool enable_translation;
    bool enable_rotation;
    bool enable_scale;
    bool enable_wireframe;
    bool show_gui;
    
    bool is_initialized;
};

void
gen_koch_showflake_buffers(Koch_Snowflake_Scene* scene,
                           std::vector<glm::vec2> curr_outline,
                           std::vector<glm::vec2> fill,
                           int depth) {
    assert(depth > 0);
    if (depth > max_recursion_depth) {
        return;
    }

    size_t next_outline_size = curr_outline.empty() ? 3 : curr_outline.size()*4;
    std::vector<glm::vec2> next_outline(next_outline_size);
    if (depth == 1) {
        glm::vec2 p1( 0.0f,  0.85f);
        glm::vec2 p2( 0.7f, -0.35f);
        glm::vec2 p3(-0.7f, -0.35f);
        next_outline[0] = p1;
        next_outline[1] = p2;
        next_outline[2] = p3;
        fill = next_outline;
    } else {
        assert(curr_outline.size() >= 3);
        
        for (int i = 0; i < curr_outline.size(); i++) {
            glm::vec2 p0 = curr_outline[i];
            glm::vec2 p1;
            if (i < curr_outline.size() - 1) {
                p1 = curr_outline[i + 1];
            } else {
                p1 = curr_outline[0];
            }
            glm::vec2 d = p1 - p0;
            glm::vec2 n(-d.y, d.x);
            n = glm::normalize(n);
            glm::vec2 q0 = p0 + d/3.0f;
            glm::vec2 m  = p0 + d/2.0f;
            glm::vec2 q1 = q0 + d/3.0f;
            glm::vec2 a = m + n*glm::length(d)*glm::sqrt(3.0f)/6.0f;
            
            next_outline[i*4]     = p0;
            next_outline[i*4 + 1] = q0;
            next_outline[i*4 + 2] = a;
            next_outline[i*4 + 3] = q1;
            
            fill.push_back(q0);
            fill.push_back(q1);
            fill.push_back(a);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, scene->outline_vbo[depth - 1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*next_outline.size(),
                 &next_outline[0].x,
                 GL_STATIC_DRAW);
    scene->outline_count[depth - 1] = (GLsizei) next_outline.size();

    glGenBuffers(1, &scene->fill_vbo[depth - 1]);
    glBindBuffer(GL_ARRAY_BUFFER, scene->fill_vbo[depth - 1]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*fill.size(),
                 &fill[0].x,
                 GL_STATIC_DRAW);
    scene->fill_count[depth - 1] = (GLsizei) fill.size();

    gen_koch_showflake_buffers(scene, next_outline, fill, depth + 1);
}

bool
initialize_scene(Koch_Snowflake_Scene* scene) {
    glGenBuffers(max_recursion_depth, scene->outline_vbo);
    glGenBuffers(max_recursion_depth, scene->fill_vbo);

    // Setup vertex buffers for each snowflake
    gen_koch_showflake_buffers(scene, {}, {}, 1);

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
        "uniform vec4 color;\n"
        "void main() {\n"
        "    frag_color = color;\n"
        "}\n";

    // Load shader from sources
    scene->shader = load_glsl_shader_from_sources(vert_shader_source, frag_shader_source);

    // Setup shader uniform location
    scene->transform_uniform = glGetUniformLocation(scene->shader, "transform");
    if (scene->transform_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `transform`");
        return false;
    }

    scene->color_uniform = glGetUniformLocation(scene->shader, "color");
    if (scene->color_uniform == -1) {
        printf("[OpenGL] failed to get uniform location `color`");
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
update_and_render_scene(Koch_Snowflake_Scene* scene, Window* window) {
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

    // Set viewport
    i32 size = window->width < window->height ? window->width : window->height;
    glViewport(window->width/2 - size/2, window->height/2 - size/2, size, size);

    // Render and clear background
    glClearColor(primary_bg_color.x, primary_bg_color.y, primary_bg_color.z, primary_bg_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // Rendering the koch snowflake
    glUseProgram(scene->shader);
    glBindBuffer(GL_ARRAY_BUFFER, scene->fill_vbo[scene->recursion_depth - 1]);
    glUniformMatrix3fv(scene->transform_uniform, 1, GL_FALSE, glm::value_ptr(transform));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    // Draw fill
    glUniform4fv(scene->color_uniform, 1, glm::value_ptr(primary_fg_color));
    if (scene->enable_wireframe) {
        glLineWidth(3);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glDrawArrays(GL_TRIANGLES, 0, scene->fill_count[scene->recursion_depth - 1]);

    // Draw outline
    glLineWidth(3);
    glUniform4f(scene->color_uniform, 0.0f, 0.0f, 0.0f, 0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, scene->outline_vbo[scene->recursion_depth - 1]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINE_LOOP, 0, scene->outline_count[scene->recursion_depth - 1]);

    // Reset states
    glLineWidth(1);
    glUseProgram(0);

    // Draw GUI
    ImGui::Begin("Koch Snowflake", &scene->show_gui, ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Recursion Depth");
    ImGui::SliderInt("", &scene->recursion_depth, 1, max_recursion_depth);
    ImGui::Text("Fill Vertex Count: %zd", scene->fill_count[scene->recursion_depth - 1]);
    ImGui::Text("Outline Vertex Count: %zd", scene->outline_count[scene->recursion_depth - 1]);
    ImGui::Checkbox("Enable translation", &scene->enable_translation);
    ImGui::Checkbox("Enable rotation", &scene->enable_rotation);
    ImGui::Checkbox("Enable scaling", &scene->enable_scale);
    ImGui::Checkbox("Wireframe mode", &scene->enable_wireframe);
    ImGui::End();
}
