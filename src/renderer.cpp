
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

    // Reset state
    glBindVertexArray(0);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(0);
    return mesh;
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
        transform->matrix = glm::translate(glm::mat4(1.0f), transform->local_position);
        transform->matrix = transform->matrix * glm::toMat4(transform->local_rotation);
        transform->matrix = glm::scale(transform->matrix,   transform->local_scale);
        transform->is_dirty = false;
    }
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
begin_3d_scene(const glm::vec4& clear_color, const glm::vec4& viewport) {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Set viewport
    glViewport(viewport.x, viewport.y, viewport.z, viewport.w);

    // Render and clear background
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void
end_3d_scene() {
    // Resetting opengl the state
    glUseProgram(0);
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
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

Basic_Material
compile_basic_material_shader() {
    const char* vertex_code = R"(
#version 330

layout(location=0) in vec3 pos;
out float illumination_amount;
uniform mat4 transform;
uniform float light_attenuation;
uniform float light_intensity;

void main() {
    vec4 world_pos = transform * vec4(pos, 1.0f);
    illumination_amount = log(light_attenuation/length(world_pos) - 0.2f)*light_intensity;
    gl_Position = world_pos;
}
)";

    const char* fragment_code = R"(
#version 330

out vec4 frag_color;
in float illumination_amount;
uniform vec4 color;

void main() {
    frag_color = color*illumination_amount;
}
)";

    Basic_Material mat = {};
    mat.shader = load_glsl_shader_from_sources(vertex_code, fragment_code);
    mat.u_transform = glGetUniformLocation(mat.shader, "transform");
    mat.u_color = glGetUniformLocation(mat.shader, "color");
    mat.u_light_intensity = glGetUniformLocation(mat.shader, "light_intensity");
    mat.u_light_attenuation = glGetUniformLocation(mat.shader, "light_attenuation");
    return mat;
}
