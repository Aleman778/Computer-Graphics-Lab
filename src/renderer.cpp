
Mesh
create_cube_mesh(glm::vec3 c, glm::vec3 d) {
    d.x /= 2.0f; d.y /= 2.0f; d.z /= 2.0f;
    static const GLsizei vertex_count = 8;
    static const float vertices[] = {
        c.x - d.x, c.y - d.y, c.z - d.z,
        c.x + d.x, c.y - d.y, c.z - d.z,
        c.x + d.x, c.y + d.y, c.z - d.z,
        c.x - d.x, c.y + d.y, c.z - d.z,
        c.x - d.x, c.y - d.y, c.z + d.z,
        c.x + d.x, c.y - d.y, c.z + d.z,
        c.x + d.x, c.y + d.y, c.z + d.z,
        c.x - d.x, c.y + d.y, c.z + d.z,
    };

    static const GLsizei index_count = 36;
    static const uint indices[] = {
        0, 1, 2, 0, 2, 3, // Front
        4, 0, 3, 4, 3, 7, // Left
        1, 5, 6, 1, 6, 2, // Right
        3, 2, 6, 3, 6, 7, // Top
        0, 1, 5, 0, 5, 4, // Bottom
        5, 4, 7, 5, 7, 6, // Back
    };

    Mesh mesh = {};
    mesh.count = index_count;
    
    // Create vertex array object
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    // Create vertex buffer
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex_count*3, vertices, GL_STATIC_DRAW);

    // Create index buffer
    glGenBuffers(1, &mesh.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint)*index_count, indices, GL_STATIC_DRAW);

    // Setup vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindVertexArray(0);
    glDisableVertexAttribArray(0);
    return mesh;
}

void
update_camera_2d(Camera2D* camera, f32 mouse_x, f32 mouse_y, f32 zoom, bool pan) {
    // Camera panning
    if (pan) {
        camera->x -= (camera->offset_x - mouse_x)/(camera->zoom + 1.0f);
        camera->y -= (camera->offset_y - mouse_y)/(camera->zoom + 1.0f);
    }
    camera->offset_x = mouse_x;
    camera->offset_y = mouse_y;

    // Camera zooming
    f32 prev_mouse_x = mouse_x/(camera->zoom + 1.0f);
    f32 prev_mouse_y = mouse_y/(camera->zoom + 1.0f);
    camera->zoom += (camera->zoom + 1.0f)*zoom*0.15f;
    if (camera->zoom < -0.98f) { // NOTE(alexander): prevent zoom increment from going to zero
        camera->zoom = -0.98f;
    }
    if (camera->zoom > 10.0f) { // NOTE(alexander): maximum zoom level
        camera->zoom = 10.0f;
    }
    camera->x += (mouse_x/(camera->zoom + 1.0f) - prev_mouse_x);
    camera->y += (mouse_y/(camera->zoom + 1.0f) - prev_mouse_y);
}

GLuint
load_glsl_shader_from_sources(const char* vertex_shader, const char* fragment_shader) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLint length = (GLint) std::strlen(vertex_shader);
    glShaderSource(vs, 1, &vertex_shader, &length);
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
    length = (GLint) std::strlen(fragment_shader);
    glShaderSource(fs, 1, &fragment_shader, &length);
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
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        GLchar* buf = new GLchar[log_length];
        glGetProgramInfoLog(program, log_length, NULL, buf);
        printf("[OpenGL] Program link error: %s", buf);
        delete[] buf;
        return false;
    }

    // Delete vertex and fragment shaders, no longer needed
    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

GLuint
load_glsl_shader_from_file(const char* filepath) {
    std::string contents = read_entire_file_to_string(filepath);
    std::string vertex_source;
    std::string fragment_source;

    usize index = contents.find("#shader ", 0);
    while (index != std::string::npos) {
        if (index != std::string::npos) {
            usize beg;
            if ((beg = contents.find("GL_VERTEX_SHADER", index)) != std::string::npos) {
                beg += 17;
                index = contents.find("#shader ", beg);
                vertex_source = contents.substr(beg, index);
            } else if ((beg = contents.find("GL_FRAGMENT_SHADER", index)) != std::string::npos) {
                beg += 19;
                index = contents.find("#shader ", beg);
                fragment_source = contents.substr(beg, index);
            } else {
                printf("Failed to parse shader file, expected shader type");
                return 0;
            }
        }
    }

    return load_glsl_shader_from_sources(vertex_source.c_str(), fragment_source.c_str());
}
