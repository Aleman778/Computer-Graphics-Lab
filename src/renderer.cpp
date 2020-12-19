
Mesh
generate_mesh_from_data(const float* vertices,
                        const uint* indices,
                        Mesh_Layout* layouts,
                        usize vertex_count,
                        usize index_count,
                        int num_layouts) {
    Mesh mesh = {};
    if (index_count == 0) {
        mesh.count = (GLsizei) vertex_count;
    } else {
        mesh.count = (GLsizei) index_count;
    }

    // Create vertex array object
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    // Create vertex buffer
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex_count, vertices, GL_STATIC_DRAW);

    // Create index buffer
    if (index_count > 0) {
        glGenBuffers(1, &mesh.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint)*index_count, indices, GL_STATIC_DRAW);
    }

    // Setup vertex layouts
    for (int i = 0; i < num_layouts; i++) {
        Mesh_Layout* layout = &layouts[i];
        glEnableVertexAttribArray(layout->location);
        glVertexAttribPointer(layout->location,
                              layout->size,
                              layout->type,
                              layout->normalized,
                              layout->stride,
                              (GLvoid*) layout->offset);
    }

    // Reset state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    for (int i = 0; i < num_layouts; i++) {
        Mesh_Layout* layout = &layouts[i];
        glDisableVertexAttribArray(layout->location);
    }

    return mesh;
}

// NOTE(alexander): optimized for space, contains only vertex positions!
Mesh
create_basic_cuboid_mesh(glm::vec3 c, glm::vec3 d) {
    const float vertices[] = {
        c.x - d.x, c.y - d.y, c.z - d.z,
        c.x + d.x, c.y - d.y, c.z - d.z,
        c.x + d.x, c.y + d.y, c.z - d.z,
        c.x - d.x, c.y + d.y, c.z - d.z,
        c.x - d.x, c.y - d.y, c.z + d.z,
        c.x + d.x, c.y - d.y, c.z + d.z,
        c.x + d.x, c.y + d.y, c.z + d.z,
        c.x - d.x, c.y + d.y, c.z + d.z,
    };

    const uint indices[] = {
        0, 1, 2, 0, 2, 3, // Front
        5, 4, 7, 5, 7, 6, // Back
        4, 0, 3, 4, 3, 7, // Left
        1, 5, 6, 1, 6, 2, // Right
        3, 2, 6, 3, 6, 7, // Top
        0, 1, 5, 0, 5, 4, // Bottom
    };

    Mesh_Layout layouts[] = { { Layout_Positions, 3, GL_FLOAT, GL_FALSE, 0, 0 } };
    return generate_mesh_from_data(vertices,
                                   indices,
                                   layouts,
                                   array_count(vertices),
                                   array_count(indices),
                                   array_count(layouts));
}

Mesh
create_cuboid_mesh(glm::vec3 c, glm::vec3 d) {
    const float vertices[] = {
        // Front
        c.x - d.x, c.y - d.y, c.z - d.z, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        c.x + d.x, c.y - d.y, c.z - d.z, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        c.x + d.x, c.y + d.y, c.z - d.z, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
        c.x - d.x, c.y + d.y, c.z - d.z, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f,

        // Back
        c.x - d.x, c.y - d.y, c.z + d.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        c.x + d.x, c.y - d.y, c.z + d.z, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        c.x + d.x, c.y + d.y, c.z + d.z, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        c.x - d.x, c.y + d.y, c.z + d.z, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,

        // Left
        c.x - d.x, c.y - d.y, c.z + d.z, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        c.x - d.x, c.y - d.y, c.z - d.z, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        c.x - d.x, c.y + d.y, c.z - d.z, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        c.x - d.x, c.y + d.y, c.z + d.z, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,

        // Right
        c.x + d.x, c.y - d.y, c.z - d.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        c.x + d.x, c.y + d.y, c.z - d.z, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        c.x + d.x, c.y - d.y, c.z + d.z, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        c.x + d.x, c.y + d.y, c.z + d.z, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,

        // Top
        c.x + d.x, c.y + d.y, c.z - d.z, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        c.x - d.x, c.y + d.y, c.z - d.z, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        c.x + d.x, c.y + d.y, c.z + d.z, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        c.x - d.x, c.y + d.y, c.z + d.z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,

        // Bottom
        c.x - d.x, c.y - d.y, c.z - d.z, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        c.x + d.x, c.y - d.y, c.z - d.z, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        c.x - d.x, c.y - d.y, c.z - d.z, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f,
        c.x + d.x, c.y - d.y, c.z - d.z, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
    };

    uint indices[36];
    for (int i = 0; i < 6; i++) {
        indices[i*6] = i;
        indices[i*6 + 1] = i*4 + 1;
        indices[i*6 + 2] = i*4 + 2;
        indices[i*6 + 3] = i*4;
        indices[i*6 + 4] = i*4 + 2;
        indices[i*6 + 5] = i*4 + 3;
    };

    Mesh_Layout layouts[] = { { Layout_Positions, 3, GL_FLOAT, GL_FALSE, 8, 0 },
                              { Layout_Texcoords, 2, GL_FLOAT, GL_FALSE, 8, 3 },
                              { Layout_Normals,   3, GL_FLOAT, GL_FALSE, 8, 5 } };

    return generate_mesh_from_data(vertices,
                                   indices,
                                   layouts,
                                   array_count(vertices),
                                   array_count(indices),
                                   array_count(layouts));
}

inline void
apply_basic_2d_shader(Basic_2D_Shader* shader) {
    glUseProgram(shader->program);
}

inline void
apply_basic_shader(Basic_Shader* shader, f32 light_intensity, f32 light_attenuation) {
    glUseProgram(shader->program);
    glUniform1f(shader->u_light_attenuation, light_attenuation);
    glUniform1f(shader->u_light_intensity, light_intensity);
}

void
update_basic_material(Basic_Material* material, const glm::mat4& transform) {
    glUniform4fv(material->shader->u_color, 1, glm::value_ptr(material->color));
    glUniformMatrix4fv(material->shader->u_transform, 1, GL_FALSE, glm::value_ptr(transform));
}

void
initialize_transform(Transform* transform, glm::vec3 pos, glm::quat rot, glm::vec3 scale) {
    transform->local_position = pos;
    transform->local_rotation = rot;
    transform->local_scale    = scale;
    transform->is_dirty       = true; // make sure to update matrix!
}

void
update_transform(Transform* transform) {
    if (transform->is_dirty) {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform->local_position);
        glm::mat4 rotation = glm::toMat4(transform->local_rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform->local_scale);
        transform->matrix = translation * rotation * scale;
        transform->is_dirty = false;
    }
}

void
draw_graphics_node(Graphics_Node* node, Camera3D* camera) {
    glBindVertexArray(node->mesh->vao);
    update_transform(&node->transform);
    glm::mat4 transform = camera->combined_matrix * node->transform.matrix;
    update_basic_material(&node->material, transform);
    glDrawElements(GL_TRIANGLES, node->mesh->count, GL_UNSIGNED_INT, 0);
}

void
initialize_camera_3d(Camera3D* camera, f32 near, f32 far, f32 aspect_ratio) {
    camera->near = near;
    camera->far = far;
    camera->aspect_ratio = aspect_ratio;
    initialize_transform(&camera->transform);
    camera->is_dirty = true;
}

void
update_camera_3d(Camera3D* camera, f32 aspect_ratio) {
    if (camera->aspect_ratio != aspect_ratio) {
        camera->aspect_ratio = aspect_ratio;
        camera->is_dirty = true;
    }

    bool is_transform_dirty = camera->transform.is_dirty;
    update_transform(&camera->transform);

    if (camera->is_dirty) {
        camera->perspective_matrix = glm::perspective(90.0f,
                                                      camera->aspect_ratio,
                                                      camera->near,
                                                      camera->far);
    }

    if (is_transform_dirty || camera->is_dirty) {
        camera->combined_matrix = camera->perspective_matrix * camera->transform.matrix;
    }

    camera->is_dirty = false;
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

void
begin_scene(const glm::vec4& clear_color, const glm::vec4& viewport, bool depth_testing) {
    // Enable depth testing
    if (depth_testing) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    // Set viewport
    glViewport((GLsizei) viewport.x, (GLsizei) viewport.y, (GLsizei) viewport.z, (GLsizei) viewport.w);

    // Render and clear background
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void
end_scene() {
    // Resetting opengl the state
    glUseProgram(0);
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1);
    glPointSize(1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

Texture
load_2d_texture_from_file(const char* filename) {
    std::ostringstream path_stream;
    path_stream << res_folder;
    path_stream << "textures/";
    path_stream << filename;
    const char* filepath = path_stream.str().c_str();

    int width, height, num_channels;
    unsigned char* data = stbi_load(filepath, &width, &height, &num_channels, 4);

    Texture texture = {};
    texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &texture.handle);
    glBindTexture(texture.target, texture.handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(texture.target, 0);

    stbi_image_free(data);
    return texture;
}

static GLuint
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

static GLuint
load_glsl_shader_from_file(const char* filename) {
    std::ostringstream path_stream;
    path_stream << res_folder;
    path_stream << "shaders/";
    path_stream << filename;
    std::string filepath = path_stream.str();
    std::string contents = read_entire_file_to_string(filepath);
    std::string vertex_source;
    std::string fragment_source;

    usize index = contents.find("#shader ", 0);
    while (index != std::string::npos) {
        usize beg_vs = contents.find("GL_VERTEX_SHADER", index);
        usize beg_fs = contents.find("GL_FRAGMENT_SHADER", index);

        if (beg_vs < beg_fs && beg_vs != std::string::npos) {
            usize beg = beg_vs + 17;
            index = contents.find("#shader ", beg);
            vertex_source = contents.substr(beg, index - beg);
        } else if (beg_fs != std::string::npos) {
            usize beg = beg_fs + 19;
            index = contents.find("#shader ", beg);
            fragment_source = contents.substr(beg, index - beg);
        } else {
            printf("Failed to parse shader file, expected shader type");
            return 0;
        }
    }

    return load_glsl_shader_from_sources(vertex_source.c_str(), fragment_source.c_str());
}

Basic_2D_Shader
compile_basic_2d_shader() {
    Basic_2D_Shader shader = {};
    shader.program = load_glsl_shader_from_file("basic_2d.glsl");
    shader.u_transform = glGetUniformLocation(shader.program, "transform");
    shader.u_color = glGetUniformLocation(shader.program, "color");
    return shader;
}

Basic_Shader
compile_basic_shader() {
    Basic_Shader shader = {};
    shader.program = load_glsl_shader_from_file("basic.glsl");
    shader.u_transform = glGetUniformLocation(shader.program, "transform");
    shader.u_color = glGetUniformLocation(shader.program, "color");
    shader.u_light_intensity = glGetUniformLocation(shader.program, "light_intensity");
    shader.u_light_attenuation = glGetUniformLocation(shader.program, "light_attenuation");
    return shader;
}

Phong_Shader
compile_phong_shader() {
    Phong_Shader shader = {};
    shader.program = load_glsl_shader_from_file("phong.glsl");
    shader.u_transform = glGetUniformLocation(shader.program, "transform");
    return shader;
}
