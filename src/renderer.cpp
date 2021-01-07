
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

    mesh.mode = GL_TRIANGLES;

    return mesh;
}

void
begin_frame(const glm::vec4& clear_color, const glm::vec4& viewport, bool depth_testing) {
    // Enable depth testing
    if (depth_testing) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    // Set viewport
    glViewport((GLsizei) viewport.x,
               (GLsizei) viewport.y,
               (GLsizei) viewport.z,
               (GLsizei) viewport.w);

    // Render and clear background
    glClearColor(clear_color.x,
                 clear_color.y,
                 clear_color.z,
                 clear_color.w);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

inline void // TODO(alexnader): move this into apply_material
apply_basic_shader(Basic_Shader* shader, f32 light_intensity, f32 light_attenuation) {
    glUseProgram(shader->program);
    glUniform1f(shader->u_light_attenuation, light_attenuation);
    glUniform1f(shader->u_light_intensity, light_intensity);
}

inline void
apply_material(const Material& material,
               Light_Setup* light_setup,
               glm::vec3 sky_color,
               f32 fog_density,
               f32 fog_gradient) {

    switch (material.type) {
        case Material_Type_Basic: {
            const Basic_Shader* shader = material.Basic.shader;
            glUseProgram(shader->program);
        } break;
        
        case Material_Type_Phong: {
            const Phong_Shader* shader = material.Phong.shader;
            glUseProgram(shader->program);
            if (light_setup) {
                glUniform3fv(shader->u_light_setup.position, 1, glm::value_ptr(light_setup->position));
                glUniform3fv(shader->u_light_setup.view_position, 1, glm::value_ptr(light_setup->view_position));
                glUniform3fv(shader->u_light_setup.color, 1, glm::value_ptr(light_setup->color));
                glUniform1f(shader->u_light_setup.ambient_intensity, light_setup->ambient_intensity);
            }

            glUniform3fv(shader->u_sky_color, 1, glm::value_ptr(sky_color));
            glUniform1f(shader->u_fog_density, fog_density);
            glUniform1f(shader->u_fog_gradient, fog_gradient);
        } break;

        case Material_Type_Sky: {
            const Sky_Shader* shader = material.Sky.shader;
            glUseProgram(shader->program);
            glUniform3fv(shader->u_fog_color, 1, glm::value_ptr(sky_color));
        } break;
    }
}

void
draw_mesh(const Mesh& mesh,
          const Material& material,
          const glm::mat4& model_matrix,
          const glm::mat4& view_matrix,
          const glm::mat4& projection_matrix,
          const glm::mat4& view_proj_matrix) {

    switch (material.type) {
        case Material_Type_Basic: {
            const Basic_Material* basic = &material.Basic;
            glUniform4fv(basic->shader->u_color, 1, glm::value_ptr(basic->color));
            
            glm::mat4 mvp_transform = model_matrix * view_proj_matrix;
            glUniformMatrix4fv(basic->shader->u_mvp_transform, 1, GL_FALSE, glm::value_ptr(mvp_transform));
        } break;

        case Material_Type_Phong: {
            const Phong_Material* phong = &material.Phong;
            glUniformMatrix4fv(phong->shader->u_model_transform, 1, GL_FALSE, glm::value_ptr(model_matrix));

            glUniform3fv(phong->shader->u_diffuse_color, 1, glm::value_ptr(phong->diffuse_color));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(phong->diffuse_map->target, phong->diffuse_map->handle);
            glUniform1i(phong->shader->u_diffuse_map, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(phong->diffuse_map->target, phong->diffuse_map->handle);
            glUniform1i(phong->shader->u_specular_map, 1);

            glUniform1f(phong->shader->u_shininess, phong->shininess);

            glm::mat4 mvp_transform = view_proj_matrix * model_matrix;
            glUniformMatrix4fv(phong->shader->u_mvp_transform, 1, GL_FALSE, glm::value_ptr(mvp_transform));

        } break;

        case Material_Type_Sky: {
            const Sky_Material* sky = &material.Sky;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(sky->map->target, sky->map->handle);
            glUniform1i(sky->shader->u_map, 0);

            // Sky should not be moved by the camera!
            glm::mat4 vp_transform = glm::mat4(view_matrix);
            vp_transform[3].x = 0.0f;
            vp_transform[3].y = 0.0f;
            vp_transform[3].z = 0.0f;
            vp_transform = projection_matrix * vp_transform;
            glUniformMatrix4fv(sky->shader->u_vp_transform, 1, GL_FALSE, glm::value_ptr(vp_transform));
        } break;
    }

    // Draw mesh
    glBindVertexArray(mesh.vao);

    if (mesh.is_two_sided) {
        glDisable(GL_CULL_FACE);
    }
    if (mesh.ibo > 0) {
        glDrawElements(mesh.mode, mesh.count, GL_UNSIGNED_SHORT, 0);
    } else {
        glDrawArrays(mesh.mode, 0, mesh.count);
    }
    if (mesh.is_two_sided) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}

void
end_frame() {
    // Resetting opengl the state
    glUseProgram(0);
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1);
    glPointSize(1);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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
load_texture_2d_from_file(const char* filename,
                          bool gen_mipmaps,
                          f32 lod_bias,
                          bool use_anisotropic_filtering,
                          f32 max_anisotropy) {
    std::ostringstream path_stream;
    path_stream << res_folder;
    path_stream << "textures/";
    path_stream << filename;
    std::string filepath = path_stream.str();

    Texture texture = {};
    
    if (filepath.length() > 4 && filepath.compare(filepath.length() - 4, 4, ".hdr") == 0) {
        int width, height;
        f32* data = load_hdr_image(filepath.c_str(), &width, &height);
        if (!data) {
            printf("cannot load HDR image `%s`\n", filepath.c_str());
            exit(0);
        }
        texture = create_texture_2d_from_data(data,
                                              width,
                                              height,
                                              true,
                                              gen_mipmaps,
                                              lod_bias,
                                              use_anisotropic_filtering,
                                              max_anisotropy);
    } else {
        
        int width, height, num_channels;
        u8* data = stbi_load(filepath.c_str(), &width, &height, &num_channels, 0);
        if (!data) {
            printf("cannot load image `%s` because %s\n", filepath.c_str(), stbi_failure_reason());
            exit(0);
        }
        texture = create_texture_2d_from_data(data,
                                              width,
                                              height,
                                              false,
                                              gen_mipmaps,
                                              lod_bias,
                                              use_anisotropic_filtering,
                                              max_anisotropy);
        stbi_image_free(data);
        
    }
    return texture;
}

Texture
create_texture_2d_from_data(void* data,
                            int width,
                            int height,
                            bool hdr_texture,
                            bool gen_mipmaps,
                            f32 lod_bias,
                            bool use_anisotropic_filtering,
                            f32 max_anisotropy) {
    Texture texture = {};
    texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &texture.handle);
    glBindTexture(texture.target, texture.handle);

    if (hdr_texture) {
        glTexImage2D(texture.target, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);
        glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    } else {
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
    }
    
    glBindTexture(texture.target, 0);
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
    GLuint program = load_glsl_shader_from_file("basic_2d.glsl");
    glUseProgram(program);
    shader.program = program;
    shader.u_color         = glGetUniformLocation(program, "color");
    shader.u_mvp_transform = glGetUniformLocation(program, "mvp_transform");
    return shader;
}

Basic_Shader
compile_basic_shader() {
    Basic_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("basic.glsl");
    glUseProgram(program);
    shader.program = program;
    shader.u_color             = glGetUniformLocation(program, "color");
    shader.u_light_intensity   = glGetUniformLocation(program, "light_intensity");
    shader.u_light_attenuation = glGetUniformLocation(program, "light_attenuation");
    shader.u_mvp_transform     = glGetUniformLocation(program, "mvp_transform");
    return shader;
}

Phong_Shader
compile_phong_shader() {
    Phong_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("phong.glsl");
    glUseProgram(program);
    shader.program = program;
    shader.u_diffuse_color = glGetUniformLocation(program, "material.diffuse_color");
    shader.u_diffuse_map   = glGetUniformLocation(program, "material.diffuse_map");
    shader.u_specular_map  = glGetUniformLocation(program, "material.specular_map");
    shader.u_shininess     = glGetUniformLocation(program, "material.shininess");
    
    shader.u_model_transform = glGetUniformLocation(program, "model_transform");
    shader.u_mvp_transform   = glGetUniformLocation(program, "mvp_transform");

    shader.u_sky_color    = glGetUniformLocation(program, "sky_color");
    shader.u_fog_density  = glGetUniformLocation(program, "fog_density");
    shader.u_fog_gradient = glGetUniformLocation(program, "fog_gradient");

    shader.u_light_setup.position          = glGetUniformLocation(program, "light_setup.position");
    shader.u_light_setup.view_position     = glGetUniformLocation(program, "light_setup.view_position");
    shader.u_light_setup.color             = glGetUniformLocation(program, "light_setup.color");
    shader.u_light_setup.ambient_intensity = glGetUniformLocation(program, "light_setup.ambient_intensity");
    return shader;
}

Sky_Shader
compile_sky_shader() {
    Sky_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("sky.glsl");
    glUseProgram(program);
    shader.program = program;
    shader.u_map          = glGetUniformLocation(program, "material.map");
    shader.u_fog_color    = glGetUniformLocation(program, "material.fog_color");
    shader.u_vp_transform = glGetUniformLocation(program, "vp_transform");
    return shader;
}
