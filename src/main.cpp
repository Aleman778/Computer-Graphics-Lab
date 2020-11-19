#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <cassert>

#include <string>
#include <vector>
#include <random>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/matrix_transform_2d.hpp>
#include <gtc/type_ptr.hpp>

#include <GL/glew.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"

typedef unsigned int uint;
typedef  int8_t   i8;
typedef uint8_t   u8;
typedef  int16_t  i16;
typedef uint16_t  u16;
typedef  int32_t  i32;
typedef uint32_t  u32;
typedef  int64_t  i64;
typedef uint64_t  u64;
typedef ptrdiff_t isize;
typedef size_t    usize;
typedef float     f32;
typedef double    f64;

static bool is_running = true;

struct Button_State {
    int half_transition_count;
    bool ended_down;
};

struct Input {
    f32 mouse_x;
    f32 mouse_y;
    f32 mouse_scroll_x;
    f32 mouse_scroll_y;
    Button_State left_mb;
    Button_State right_mb;
    Button_State middle_mb;
};

bool
was_pressed(Button_State* state) {
    return state->half_transition_count > 1 ||
        (state->half_transition_count == 1 && state->ended_down);
}

struct Window {
    i32 width;
    i32 height;
    Input input;
};

struct Camera2D {
    f32 offset_x;
    f32 offset_y;
    f32 x;
    f32 y;
    f32 zoom;
};

struct Mesh {
    GLuint vbo;
    GLuint vao;
    GLsizei count;
};

inline Mesh
create_2d_mesh(const std::vector<glm::vec2>& data) {
    Mesh mesh = {};
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*data.size(),
                 &data[0].x,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindVertexArray(0);
    mesh.count = (GLsizei) data.size();
    return mesh;
}

inline void
update_2d_mesh(Mesh* mesh, const std::vector<glm::vec2>& data) {
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(glm::vec2)*data.size(),
                 &data[0].x,
                 GL_STATIC_DRAW);
    glBindVertexArray(0);
    mesh->count = (GLsizei) data.size();
}

void
update_camera_2d(Window* window, Camera2D* camera) {
    // Camera panning
    if (window->input.right_mb.ended_down) {
        camera->x -= (camera->offset_x - window->input.mouse_x)/(camera->zoom + 1.0f);
        camera->y -= (camera->offset_y - window->input.mouse_y)/(camera->zoom + 1.0f);
    }
    camera->offset_x = window->input.mouse_x;
    camera->offset_y = window->input.mouse_y;

    // Camera zooming
    f32 prev_mouse_x = window->input.mouse_x/(camera->zoom + 1.0f);
    f32 prev_mouse_y = window->input.mouse_y/(camera->zoom + 1.0f);
    camera->zoom += (camera->zoom + 1.0f)*window->input.mouse_scroll_y*0.15f;
    if (camera->zoom < -0.98f) { // NOTE(alexander): prevent zoom increment from going to zero
        camera->zoom = -0.98f;
    }
    if (camera->zoom > 10.0f) { // NOTE(alexander): maximum zoom level
        camera->zoom = 10.0f;
    }
    camera->x += (window->input.mouse_x/(camera->zoom + 1.0f) - prev_mouse_x);
    camera->y += (window->input.mouse_y/(camera->zoom + 1.0f) - prev_mouse_y);
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

// #include "koch_snowflake.cpp"
#include "triangulation.cpp"

void
opengl_debug_callback(GLenum source,
                      GLenum type,
                      GLenum id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message,
                      const void* user_params) {
    std::string header("[OpenGL] ");
    switch (severity) {
        case GL_DEBUG_SEVERITY_LOW: {
            header.append("<low severity> ");
        } break;

        case GL_DEBUG_SEVERITY_MEDIUM: {
            header.append("<medium severity> ");
        } break;

        case GL_DEBUG_SEVERITY_HIGH: {
            header.append("<high severity> ");
        } break;
    }

    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            printf("%s Error: %s\n", header.c_str(), message);
            break;

        case GL_DEBUG_TYPE_PERFORMANCE:
            printf("%s Performance issue: %s\n", header.c_str(), message);
            break;
        default: break;
    }
}

void
window_mouse_callback(GLFWwindow* glfw_window, int button, int action, int mods) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: {
                window->input.left_mb.ended_down = action == GLFW_PRESS;
                window->input.left_mb.half_transition_count++;
            } break;

            case GLFW_MOUSE_BUTTON_RIGHT: {
                window->input.right_mb.ended_down = action == GLFW_PRESS;
                window->input.right_mb.half_transition_count++;
            } break;

            case GLFW_MOUSE_BUTTON_MIDDLE: {
                window->input.middle_mb.ended_down = action == GLFW_PRESS;
                window->input.middle_mb.half_transition_count++;
            } break;
        }
    }
}

void
window_scroll_callback(GLFWwindow* glfw_window, double xoffset, double yoffset) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        window->input.mouse_scroll_x = (f32) xoffset;
        window->input.mouse_scroll_y = (f32) yoffset;
    }
}

void
window_cursor_pos_callback(GLFWwindow* glfw_window, double xpos, double ypos) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        window->input.mouse_x = (f32) xpos;
        window->input.mouse_y = (f32) ypos;
    }
}

void
window_size_callback(GLFWwindow* glfw_window, i32 width, i32 height) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        window->width = width;
        window->height = height;
    }
}

int
main() {
    if (!glfwInit()) {
        return -1;
    }

    // Setup window information
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#ifndef __APPLE__
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 8);

    // Creating the window
    Window window = {};
    window.width = 1280;
    window.height = 720;

    GLFWwindow* glfw_window = glfwCreateWindow(window.width, window.height, "D7045E Lab", 0, 0);
    glfwSetWindowUserPointer(glfw_window, &window);
    glfwMakeContextCurrent(glfw_window);
    if (!glfw_window) {
        glfwTerminate();
        return -1;
    }

    // Initializing GLEW, OpenGL 3.0+ is a hard requirement
    GLenum result = glewInit();
    assert(result == GLEW_OK);
    if (!(GLEW_VERSION_3_0)) {
        printf("OpenGL 3.0+ is not supported on this hardware!\n");
        glfwTerminate();
        return -1;
    }

    // Enable opengl debug callbacks, if available
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    if (glDebugMessageCallback) {
        glDebugMessageCallback(opengl_debug_callback, 0);
        GLuint unused_ids;
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &unused_ids, true);
    }

    // Setup glfw callbacks
    // glfwSetKeyCallback(glfw_window,         key_callback);
    glfwSetMouseButtonCallback(glfw_window, window_mouse_callback);
    glfwSetCursorPosCallback(glfw_window,   window_cursor_pos_callback);
    // glfwSetCursorEnterCallback(glfw_window, enter_callback);
    glfwSetScrollCallback(glfw_window,      window_scroll_callback);
    glfwSetWindowSizeCallback(glfw_window,  window_size_callback);

    // Setup ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfwGL3_Init(glfw_window, false);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("../roboto.ttf", 20);

    // Koch_Snowflake_Scene scene = {};
    Triangulation_Scene scene = {};

    // Programs main loop
    while (!glfwWindowShouldClose(glfw_window) && is_running) {
        ImGui_ImplGlfwGL3_NewFrame();

        update_and_render_scene(&scene, &window);

        ImGui::Render();

        // reset input states mouse scroll
        window.input.mouse_scroll_x = 0;
        window.input.mouse_scroll_y = 0;
        window.input.left_mb.half_transition_count = 0;
        window.input.right_mb.half_transition_count = 0;
        window.input.middle_mb.half_transition_count = 0;
        
        glfwSwapBuffers(glfw_window);
        glfwPollEvents();
    }

    glfwDestroyWindow(glfw_window);
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();
    return 0;
}
