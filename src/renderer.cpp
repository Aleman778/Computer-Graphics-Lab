

Mesh
create_mesh_from_builder(Mesh_Builder* mb) {
    Mesh mesh = {};
    GLsizei vertex_count = (GLsizei) mb->vertices.size();
    GLsizei index_count  = (GLsizei) mb->indices.size();
    
    if (index_count == 0) mesh.count = vertex_count;
    else                  mesh.count = index_count;

    // Create vertex array object
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    // Create vertex buffer
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*vertex_count, &mb->vertices[0].pos.x, GL_STATIC_DRAW);

    // Create index buffer
    if (index_count > 0) {
        glGenBuffers(1, &mesh.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16)*index_count, &mb->indices[0], GL_STATIC_DRAW);
    }

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(f32), (GLvoid*) (0*sizeof(f32)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(f32), (GLvoid*) (3*sizeof(f32)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(f32), (GLvoid*) (5*sizeof(f32)));

    // Reset state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    return mesh;
}

inline void
apply_shader(Shader_Base* shader, Light_Setup* light_setup, glm::vec3 sky_color, f32 fog_density, f32 fog_gradient) {
    glUseProgram(shader->program);
    if (light_setup) {
        glUniform3fv(shader->u_light_setup.position, 1, glm::value_ptr(light_setup->position));
        glUniform3fv(shader->u_light_setup.view_position, 1, glm::value_ptr(light_setup->view_position));
        glUniform3fv(shader->u_light_setup.color, 1, glm::value_ptr(light_setup->color));
        glUniform1f(shader->u_light_setup.ambient_intensity, light_setup->ambient_intensity);
    }

    if (shader->u_sky_color != -1) {
        glUniform3fv(shader->u_sky_color, 1, glm::value_ptr(sky_color));
    }
    
    if (shader->u_fog_density != -1) {
        glUniform1f(shader->u_fog_density, fog_density);
    }
    
    if (shader->u_fog_gradient != -1) {
        glUniform1f(shader->u_fog_gradient, fog_gradient);
    }
}

inline void
apply_basic_shader(Basic_Shader* shader, f32 light_intensity, f32 light_attenuation) {
    glUseProgram(shader->base.program);
    glUniform1f(shader->u_light_attenuation, light_attenuation);
    glUniform1f(shader->u_light_intensity, light_intensity);
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
        transform->matrix = rotation * translation * scale;
        transform->is_dirty = false;
    }
}

void
draw_graphics_node(Graphics_Node* node, Camera_3D* camera) {
    // Update transform of the node
    update_transform(&node->transform);
    glm::mat4 mvp_transform = camera->combined_matrix * node->transform.matrix;

    // Apply material
    Material* material = &node->material;
    glUniformMatrix4fv(material->shader->u_mvp_transform, 1, GL_FALSE, glm::value_ptr(mvp_transform));
    switch (material->type) {
        case Material_Type_Basic: {
            Basic_Material* basic = &material->Basic;
            glUniform4fv(basic->shader->u_color, 1, glm::value_ptr(basic->color));
        } break;

        case Material_Type_Phong: {
            Phong_Material* phong = &material->Phong;
            glUniformMatrix4fv(material->shader->u_model_transform, 1,
                               GL_FALSE, glm::value_ptr(node->transform.matrix));
            glUniform4fv(phong->shader->u_object_color, 1, glm::value_ptr(phong->object_color));

            if (phong->main_texture) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(phong->main_texture->target, phong->main_texture->handle);
                glUniform1i(phong->shader->u_sampler, 0);
            }
        } break;
    }

    // Draw mesh
    glBindVertexArray(node->mesh->vao);
    glDrawElements(GL_TRIANGLES, node->mesh->count, GL_UNSIGNED_SHORT, 0);
}

void
initialize_camera_3d(Camera_3D* camera, f32 near, f32 far, f32 aspect_ratio) {
    camera->near = near;
    camera->far = far;
    camera->aspect_ratio = aspect_ratio;
    initialize_transform(&camera->transform);
    camera->is_dirty = true;
}

void
update_camera_3d(Camera_3D* camera, f32 aspect_ratio) {
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
initialize_fps_camera(Fps_Camera* camera, f32 near, f32 far, f32 sensitivity, f32 aspect_ratio) {
    initialize_camera_3d(&camera->base, near, far, aspect_ratio);
    camera->sensitivity = sensitivity;
}

void
update_fps_camera(Fps_Camera* camera, Input* input, int width, int height) {
    Transform* transform = &camera->base.transform;

    // Looking around, whenever mouse is locked, left click in window (lock), escape (unlock).
    if (input->mouse_locked) {
        f32 delta_x = input->mouse_x - (f32) (width / 2);
        f32 delta_y = input->mouse_y - (f32) (height / 2);

        bool is_dirty = false;
        if (delta_x > 0.1f || delta_x < -0.1f) {
            camera->rotation_x += delta_x * camera->sensitivity;
            is_dirty = true;
        }

        if (delta_y > 0.1f || delta_y < -0.1f) {
            camera->rotation_y += delta_y * camera->sensitivity;
            if (camera->rotation_y < -1.5f) {
                camera->rotation_y = -1.5f;
            }

            if (camera->rotation_y > 1.2f) {
                camera->rotation_y = 1.2f;
            }
            is_dirty = true;
        }

        if (is_dirty) {
            glm::quat rot_x(glm::vec3(0.0f, camera->rotation_x, 0.0f));
            glm::quat rot_y(glm::vec3(camera->rotation_y, 0.0f, 0.0f));
            transform->local_rotation = rot_y * rot_x;
            transform->is_dirty = true;
        }
    }

    // Walking
    glm::vec3* pos = &transform->local_position;

    float speed = 0.02f;
    if (input->shift_key.ended_down) {
        speed = 0.08f;
    }

    f32 half_pi = glm::pi<f32>()/2.0f;
    glm::vec3 forward(cos(camera->rotation_x + half_pi)*speed, 0.0f, sin(camera->rotation_x + half_pi)*speed);
    glm::vec3 right(cos(camera->rotation_x)*speed, 0.0f, sin(camera->rotation_x)*speed); 

    if (input->w_key.ended_down) {
        *pos += forward;
        transform->is_dirty = true;
    }
    
    if (input->a_key.ended_down) {
        *pos += right;
        transform->is_dirty = true;
    }
    
    if (input->s_key.ended_down) {
        *pos -= forward;
        transform->is_dirty = true;
    }
    
    if (input->d_key.ended_down) {
        *pos -= right;
        transform->is_dirty = true;
    }

    // Enable/ disable mouse locking
    if (was_pressed(&input->left_mb)) {
        input->mouse_locked = true;
    }

    if (was_pressed(&input->escape_key)){
        input->mouse_locked = false;
    }

    // Update the actual camera matrices
    f32 aspect_ratio = 0.0f;
    if (width != 0 && height != 0) {
        aspect_ratio = ((f32) width)/((f32) height);
    }
    update_camera_3d(&camera->base, aspect_ratio);
}

void
update_camera_2d(Camera_2D* camera, Input* input) {
    // Camera panning
    if (input->right_mb.ended_down) {
        camera->x -= (camera->offset_x - input->mouse_x)/(camera->zoom + 1.0f);
        camera->y -= (camera->offset_y - input->mouse_y)/(camera->zoom + 1.0f);
    }
    camera->offset_x = input->mouse_x;
    camera->offset_y = input->mouse_y;

    // Camera zooming
    f32 prev_mouse_x = input->mouse_x/(camera->zoom + 1.0f);
    f32 prev_mouse_y = input->mouse_y/(camera->zoom + 1.0f);
    camera->zoom += (camera->zoom + 1.0f)*input->mouse_scroll_y*0.15f;
    if (camera->zoom < -0.98f) { // NOTE(alexander): prevent zoom increment from going to zero
        camera->zoom = -0.98f;
    }
    if (camera->zoom > 10.0f) { // NOTE(alexander): maximum zoom level
        camera->zoom = 10.0f;
    }
    camera->x += (input->mouse_x/(camera->zoom + 1.0f) - prev_mouse_x);
    camera->y += (input->mouse_y/(camera->zoom + 1.0f) - prev_mouse_y);
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
generate_white_2d_texture() {
    u8 data[4*4*4]; // RGBA 4*4 texture
    for (int i = 0; i < array_count(data); i++) data[i] = (u8) 255;

    Texture texture = {};
    texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &texture.handle);
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(texture.target, 0);

    return texture;
}

Texture
load_2d_texture_from_file(const char* filename,
                          bool gen_mipmaps,
                          f32 lod_bias,
                          bool use_anisotropic_filtering, // requires gen_mipmaps=true
                          f32 max_anisotropy) {
    std::ostringstream path_stream;
    path_stream << res_folder;
    path_stream << "textures/";
    path_stream << filename;
    std::string filepath = path_stream.str();

    int width, height, num_channels;
    u8* data = stbi_load(filepath.c_str(), &width, &height, &num_channels, 0);
    if (!data) {
        printf("cannot load image `%s` because %s\n", filepath.c_str(), stbi_failure_reason());
        exit(0);
    }

    Texture texture = {};
    texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &texture.handle);
    glBindTexture(texture.target, texture.handle);
    glTexImage2D(texture.target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (gen_mipmaps) {
        glGenerateMipmap(texture.target);
        glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        if (GLEW_ARB_texture_filter_anisotropic && use_anisotropic_filtering) {
            f32 amount = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &amount);
            amount = amount > max_anisotropy ? max_anisotropy : amount;
            glTexParameterf(texture.target, GL_TEXTURE_MAX_ANISOTROPY, amount);
            glTexParameterf(texture.target, GL_TEXTURE_LOD_BIAS, 0);
        } else {
            glTexParameterf(texture.target, GL_TEXTURE_LOD_BIAS, lod_bias);
        }
    } else {
        glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

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

static void
setup_shader_base_uniforms(Shader_Base* shader_base) {
    GLuint program = shader_base->program;
    shader_base->u_model_transform = glGetUniformLocation(program, "model_transform");
    shader_base->u_mvp_transform = glGetUniformLocation(program, "mvp_transform");

    shader_base->u_sky_color = glGetUniformLocation(program, "sky_color");
    shader_base->u_fog_density = glGetUniformLocation(program, "fog_density");
    shader_base->u_fog_gradient = glGetUniformLocation(program, "fog_gradient");

    shader_base->u_light_setup.position = glGetUniformLocation(program, "light_setup.position");
    shader_base->u_light_setup.view_position = glGetUniformLocation(program, "light_setup.view_position");
    shader_base->u_light_setup.color = glGetUniformLocation(program, "light_setup.color");
    shader_base->u_light_setup.ambient_intensity = glGetUniformLocation(program, "light_setup.ambient_intensity");
}

Basic_2D_Shader
compile_basic_2d_shader() {
    Basic_2D_Shader shader = {};
    shader.base.program = load_glsl_shader_from_file("basic_2d.glsl");
    shader.base.u_mvp_transform = glGetUniformLocation(shader.base.program, "mvp_transform");
    shader.base.u_sky_color     = -1;
    shader.base.u_fog_density   = -1;
    shader.base.u_fog_gradient  = -1;
    shader.u_color              = glGetUniformLocation(shader.base.program, "color");
    return shader;
}

Basic_Shader
compile_basic_shader() {
    Basic_Shader shader = {};
    shader.base.program = load_glsl_shader_from_file("basic.glsl");
    shader.base.u_mvp_transform = glGetUniformLocation(shader.base.program, "mvp_transform");
    shader.base.u_sky_color     = -1;
    shader.base.u_fog_density   = -1;
    shader.base.u_fog_gradient  = -1;
    shader.u_color              = glGetUniformLocation(shader.base.program, "color");
    shader.u_light_intensity    = glGetUniformLocation(shader.base.program, "light_intensity");
    shader.u_light_attenuation  = glGetUniformLocation(shader.base.program, "light_attenuation");
    return shader;
}

Phong_Shader
compile_phong_shader() {
    Phong_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("phong.glsl");
    shader.base.program = program;
    shader.u_object_color = glGetUniformLocation(program, "object_color");
    shader.u_sampler = glGetUniformLocation(program, "sampler");
    setup_shader_base_uniforms(&shader.base);
    return shader;
}

