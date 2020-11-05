
/***************************************************************************
 * Lab assignment 1 - Alexander Mennborg
 * Koch Snowflake - https://en.wikipedia.org/wiki/Koch_snowflake
 ***************************************************************************/

const int MAX_RECURSION_DEPTH = 7;

const char* vert_shader_source =
    "#version 330\n"
    "layout(location=0) in vec2 pos;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 1.0f, 1.0f);\n"
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

    int recursion_depth;
    bool show_gui;
    bool is_initialized;
};

/*
  NOTE(alexander): point in triangle algorithm from:
  https://stackoverflow.com/questions/2049582/how-to-determine-if-a-point-is-in-a-2d-triangle
*/
bool
point_in_triangle(glm::vec2 pt, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3) {
    auto sign = [](glm::vec2 p1, glm::vec2 p2, glm::vec2 p3) -> float {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };

    float d1 = sign(pt, p1, p2);
    float d2 = sign(pt, p2, p3);
    float d3 = sign(pt, p3, p1);
    return !((d1 < 0) || (d2 < 0) || (d3 < 0) &&
             (d1 > 0) || (d2 > 0) || (d3 > 0));
}


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


// void
// gen_koch_showflake_buffers(Koch_Snowflake_Scene* scene,
//                            std::vector<glm::vec2>* curr_triangles,
//                            int depth) {
//     assert(depth > 0);
//     if (depth > 7) {
//         return;
//     }
    
//     auto gen_triangles = [](std::vector<glm::vec2>* next_triangles,
//                             glm::vec2 p0,
//                             glm::vec2 p1,
//                             glm::vec2 b,
//                             bool gen_extruding_triangle) {
//         glm::vec2 q0 = p0 + (p1 - p0)/3.0f;
//         glm::vec2 m  = p0 + (p1 - p0)/2.0f;
//         glm::vec2 q1 = q0 + (p1 - p0)/3.0f;
//         glm::vec2 a = m - glm::normalize(b - m)*(glm::length(p1 - p0)*(glm::sqrt(3.0f)/6.0f));


//         next_triangles->push_back(q0);
//         next_triangles->push_back(p0 + (b - p0)/3.0f);
//         next_triangles->push_back(p0);
        
        
//         next_triangles->push_back(p0 + (b - p0)/3.0f);
//         next_triangles->push_back(q0);
//         next_triangles->push_back(p0);
 
//         // Extruding triangle
//         if (gen_extruding_triangle) {
//             next_triangles->push_back(q1);
//             next_triangles->push_back(a);
//             next_triangles->push_back(q0);
   

//             next_triangles->push_back(a);
//             next_triangles->push_back(q0);
//             next_triangles->push_back(q1);
//         }
        
//         next_triangles->push_back(p1);
//         next_triangles->push_back(q1);
//         next_triangles->push_back(p1 + (b - p1)/3.0f);
//     };

//     // NOTE(alexander): the actual koch snowflake fractal curve (line loop).
//     std::vector<glm::vec2> koch_snowflake_vertices;

//     // NOTE(alexander): helper triangles used to generate next recursive step.
//     std::vector<glm::vec2> next_triangles;

//     if (depth == 1) {
//         glm::vec2 p1( 0.0f,  0.9f);
//         glm::vec2 p2( 0.8f, -0.5f);
//         glm::vec2 p3(-0.8f, -0.5f);
//         koch_snowflake_vertices.push_back(p1);
//         koch_snowflake_vertices.push_back(p2);
//         koch_snowflake_vertices.push_back(p3);

//         koch_snowflake_vertices.push_back(p2);
//         koch_snowflake_vertices.push_back(p3);
//         koch_snowflake_vertices.push_back(p1);
        
//         koch_snowflake_vertices.push_back(p3);
//         koch_snowflake_vertices.push_back(p1);
//         koch_snowflake_vertices.push_back(p2);
//         next_triangles = koch_snowflake_vertices;
//     } else {
//         assert(curr_triangles->size() >= 3 && "curr_triangles must have atleast three koch_snowflake_vertices");
//         for (int i = 0; i < curr_triangles->size(); i += 3) {
//             glm::vec2 p0 = (*curr_triangles)[i];
//             glm::vec2 p1 = (*curr_triangles)[i + 1];
//             glm::vec2 b = (*curr_triangles)[i + 2];
//             // next_triangles.push_back(p0);
//             // next_triangles.push_back(p1);
//             // next_triangles.push_back(p2);
//             bool pointing_upward = p0.y > p1.y;
//             pointing_upward = true;
//             // if (depth < 4 || pointing_upward) gen_triangles(&next_triangles, p0, p1, p2, true);
//             // gen_triangles(&next_triangles, p1, p2, p0, depth == 2 && depth < 4);
//             // if (depth < 4 || pointing_upward) gen_triangles(&next_triangles, p2, p0, p1, true);
//             gen_triangles(&next_triangles, p0, p1, b, true);
            
//             glm::vec2 q0 = p0 + (p1 - p0)/3.0f;
//             glm::vec2 m  = p0 + (p1 - p0)/2.0f;
//             glm::vec2 q1 = q0 + (p1 - p0)/3.0f;
//             glm::vec2 a = m - (b - m)*glm::sqrt(3.0f)/6.0f; // height of equilateral triangle
            
//             koch_snowflake_vertices.push_back(p0);
//             koch_snowflake_vertices.push_back(q0);
//             koch_snowflake_vertices.push_back(a);
//             koch_snowflake_vertices.push_back(q1);
//         }
//     }

//     // FIXME(alexander): remove this just to test the next triangles!
//     // koch_snowflake_vertices = next_triangles;

//     // Generate buffers for the koch snowflake 
//     glGenBuffers(1, &scene->vertex_buffers[depth - 1]);
//     glBindBuffer(GL_ARRAY_BUFFER, scene->vertex_buffers[depth - 1]);
//     glBufferData(GL_ARRAY_BUFFER,
//                  sizeof(glm::vec2)*koch_snowflake_vertices.size(),
//                  &koch_snowflake_vertices[0].x,
//                  GL_STATIC_DRAW);
//     scene->vertex_count[depth - 1] = (GLsizei) koch_snowflake_vertices.size();
//     gen_koch_showflake_buffers(scene, &next_triangles, depth + 1);
//     return;
// }

void
initialize_scene(Koch_Snowflake_Scene* scene) {
    // Setup vertex buffers for each snowflake
    gen_koch_showflake_buffers(scene, NULL, 1);

    // Setup vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLint length = (GLint) std::strlen(vert_shader_source);
    glShaderSource(vs, 1, &vert_shader_source, &length);
    glCompileShader(vs);
    opengl_log_shader_compilation(vs);

    // Setup fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    length = (GLint) std::strlen(frag_shader_source);
    glShaderSource(fs, 1, &frag_shader_source, &length);
    glCompileShader(fs);
    opengl_log_shader_compilation(fs);

    // Create shader program
    scene->shader = glCreateProgram();
    glAttachShader(scene->shader, vs);
    glAttachShader(scene->shader, fs);
    glLinkProgram(scene->shader);
    opengl_log_program_linking(scene->shader);

    scene->recursion_depth = 1;
    scene->show_gui = false;
    scene->is_initialized = true;
}

void
update_and_render_scene(Koch_Snowflake_Scene* scene) {
    if (!scene->is_initialized) {
        initialize_scene(scene);
    }

    // Render and clear background
    glClearColor(1.0f, 0.99f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render the koch snowflake
    glBindBuffer(GL_ARRAY_BUFFER, scene->vertex_buffers[scene->recursion_depth - 1]);
    glUseProgram(scene->shader);
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
    ImGui::End();
}
