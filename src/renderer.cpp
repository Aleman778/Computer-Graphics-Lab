
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

    printf("create_mesh ... vertex_count = %d, index_count = %d\n", (int) vertex_count, (int) index_count);

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

inline void
apply_shader(Shader_Base* shader, Light_Setup* light_setup, glm::vec3 sky_color, f32 fog_density, f32 fog_gradient) {
    glUseProgram(shader->program);
    if (light_setup && shader->u_light_setup.position != -1) {
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

            glUniform3fv(phong->shader->u_diffuse_color, 1, glm::value_ptr(phong->diffuse_color));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(phong->diffuse_map->target, phong->diffuse_map->handle);
            glUniform1i(phong->shader->u_diffuse_map, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(phong->diffuse_map->target, phong->diffuse_map->handle);
            glUniform1i(phong->shader->u_specular_map, 1);

            glUniform1f(phong->shader->u_shininess, phong->shininess);

        } break;

        case Material_Type_Sky: {
            Sky_Material* sky = &material->Sky;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(sky->map->target, sky->map->handle);
            glUniform1i(sky->shader->u_map, 0);
            glUniform3fv(sky->shader->u_color, 1, glm::value_ptr(sky->color));
        } break;
    }

    // Draw mesh
    assert(node->mesh && "missing mesh information");
    glBindVertexArray(node->mesh->vao);

    if (node->mesh->disable_culling) {
        glDisable(GL_CULL_FACE);
    }
    if (node->mesh->ibo > 0) {
        glDrawElements(node->mesh->mode, node->mesh->count, GL_UNSIGNED_SHORT, 0);
    } else {
        glDrawArrays(node->mesh->mode, 0, node->mesh->count);
    }
    if (node->mesh->disable_culling) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}

void
initialize_camera_3d(Camera_3D* camera, f32 fov, f32 near, f32 far, f32 aspect_ratio) {
    camera->fov = fov;
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
        camera->perspective_matrix = glm::perspective(camera->fov,
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

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
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
    glDisable(GL_CULL_FACE);
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

static f32
fade(f32 t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static f32
grad(int hash, f32 x, f32 y, f32 z) {
    switch (hash & 0xf) {
        case 0x0: return  x + y;
        case 0x1: return -x + y;
        case 0x2: return  x - y;
        case 0x3: return -x - y;
        case 0x4: return  x + z;
        case 0x5: return -x + z;
        case 0x6: return  x - z;
        case 0x7: return -x - z;
        case 0x8: return  y + z;
        case 0x9: return -y + z;
        case 0xa: return  y - z;
        case 0xb: return -y - z;
        case 0xc: return  y + x;
        case 0xd: return -y + z;
        case 0xe: return  y - x;
        case 0xf: return -y - z;
        default:  return 0; // never happens
    }
}

f32
lerp(f32 t, f32 a, f32 b) {
    return a + t * (b - a);
}

static int*
generate_perlin_permutations() {
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> random(0.0f, 1.0f);

    int* permutations = new int[512];
    for (int i = 0; i < 256; i++) {
        permutations[i] = i;
    }
    for (int i = 0; i < 256; i++) {
        int index = i + random(rng) * (256 - i);
        int temp = permutations[i];
        permutations[i] = permutations[index];
        permutations[index] = temp;
    }
    for (int i = 0; i < 256; i++) {
        permutations[256+i] = permutations[i];
    }
    // printf("generated_permutations = {\n");
    // for (int i = 0; i < 256; i++) {
    //     printf("%d, ", permutations[i]);
    //     if (i % 20 == 19) {
    //         printf("\n");
    //     }
    // }
    // printf("\n}\n");
    return permutations;
}

f32 // NOTE: implementation based on https://adrianb.io/2014/08/09/perlinnoise.html
perlin_noise(f32 x, f32 y, f32 z) {
    static int* p = generate_perlin_permutations();

    int xi = (int) x & 0xff;
    int yi = (int) y & 0xff;
    int zi = (int) z & 0xff;
    x -= (int) x;
    y -= (int) y;
    z -= (int) z;
    f32 u = fade(x);
    f32 v = fade(y);
    f32 w = fade(z);

    int aaa = p[p[p[xi    ] + yi    ] + zi    ];
    int aba = p[p[p[xi    ] + yi + 1] + zi    ];
    int aab = p[p[p[xi    ] + yi    ] + zi + 1];
    int abb = p[p[p[xi    ] + yi + 1] + zi + 1];
    int baa = p[p[p[xi + 1] + yi    ] + zi    ];
    int bba = p[p[p[xi + 1] + yi + 1] + zi    ];
    int bab = p[p[p[xi + 1] + yi    ] + zi + 1];
    int bbb = p[p[p[xi + 1] + yi + 1] + zi + 1];

    f32 x1, x2, y1, y2;
    x1 = lerp(u, grad(aaa, x, y,     z), grad(baa, x - 1, y,     z));
    x2 = lerp(u, grad(aba, x, y - 1, z), grad(bba, x - 1, y - 1, z));
    y1 = lerp(v, x1, x2);

    x1 = lerp(u, grad(aab, x, y,     z - 1), grad(bab, x - 1, y,     z - 1));
    x2 = lerp(u, grad(abb, x, y - 1, z - 1), grad(bbb, x - 1, y - 1, z - 1));
    y2 = lerp(v, x1, x2);

    return (lerp(w, y1, y2) + 1)/2;
}

f32
octave_perlin_noise(f32 x, f32 y, f32 z, int octaves, f32 persistance) {
    f32 total = 0.0f;
    f32 frequency = 1.0f;
    f32 amplitude = 1.0f;
    f32 max_value = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += perlin_noise(x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistance;
        frequency *= 2;
    }

    return total/max_value;
}

f32
sample_point_at(Height_Map* map, f32 x, f32 y) {
    int x0 = x*map->scale_x;
    int y0 = y*map->scale_y;

    if (x0 < 0) x0 = 0; if (x0 >= map->width)  x0 = map->width  - 1;
    if (y0 < 0) y0 = 0; if (y0 >= map->height) y0 = map->height - 1;
    
    int x1 = x0 <= map->width  - 1 ? x0 + 1 : map->width  - 1;
    int y1 = y0 <= map->height - 1 ? y0 + 1 : map->height - 1;

    f32 h1 = map->data[x0+y0*map->width];
    f32 h2 = map->data[x1+y0*map->width];
    f32 h3 = map->data[x0+y1*map->width];
    f32 h4 = map->data[x1+y1*map->width];

    f32 u = x*map->scale_x - x0;
    f32 v = y*map->scale_y - y0;
    f32 hx0 = lerp(u, h1, h2);
    f32 hx1 = lerp(u, h3, h4);
    return lerp(v, hx0, hx1);
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
    GLuint program = load_glsl_shader_from_file("basic_2d.glsl");
    glUseProgram(program);
    shader.base.program = program;
    shader.base.u_mvp_transform = glGetUniformLocation(program, "mvp_transform");
    shader.base.u_sky_color     = -1;
    shader.base.u_fog_density   = -1;
    shader.base.u_fog_gradient  = -1;
    shader.u_color              = glGetUniformLocation(program, "color");
    return shader;
}

Basic_Shader
compile_basic_shader() {
    Basic_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("basic.glsl");
    glUseProgram(program);
    shader.base.program = program;
    shader.base.u_mvp_transform = glGetUniformLocation(program, "mvp_transform");
    shader.base.u_sky_color     = -1;
    shader.base.u_fog_density   = -1;
    shader.base.u_fog_gradient  = -1;
    shader.u_color              = glGetUniformLocation(program, "color");
    shader.u_light_intensity    = glGetUniformLocation(program, "light_intensity");
    shader.u_light_attenuation  = glGetUniformLocation(program, "light_attenuation");
    return shader;
}

Phong_Shader
compile_phong_shader() {
    Phong_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("phong.glsl");
    glUseProgram(program);
    shader.base.program = program;
    shader.u_diffuse_color = glGetUniformLocation(program, "material.diffuse_color");
    shader.u_diffuse_map = glGetUniformLocation(program, "material.diffuse_map");
    shader.u_specular_map = glGetUniformLocation(program, "material.specular_map");
    shader.u_shininess = glGetUniformLocation(program, "material.shininess");
    setup_shader_base_uniforms(&shader.base);
    return shader;
}

Sky_Shader
compile_sky_shader() {
    Sky_Shader shader = {};
    GLuint program = load_glsl_shader_from_file("sky.glsl");
    glUseProgram(program);
    shader.base.program = program;
    shader.base.u_mvp_transform = glGetUniformLocation(program, "mvp_transfor2m");
    shader.base.u_sky_color    = -1;
    shader.base.u_fog_density  = -1;
    shader.base.u_fog_gradient = -1;
    shader.base.u_light_setup.position = -1;
    shader.u_map   = glGetUniformLocation(program, "material.map");
    shader.u_color = glGetUniformLocation(program, "material.color");
    return shader;
}
