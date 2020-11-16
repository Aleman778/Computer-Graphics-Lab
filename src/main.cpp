#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cmath>

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

void window_size_callback(GLFWwindow* window, i32 width, i32 height) {
    void* scene = glfwGetWindowUserPointer(window);
    if (scene) scene_window_size_callback(scene, width, height);
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
    i32 width = 1280;
    i32 height= 720;
    GLFWwindow* window = glfwCreateWindow(width, height, "D7045E Lab", 0, 0);
    glfwMakeContextCurrent(window);
    if (!window) {
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
    // glfwSetKeyCallback(window,         key_callback);
    // glfwSetMouseButtonCallback(window, mouse_callback);
    // glfwSetCursorPosCallback(window,   cursor_pos_callback);
    // glfwSetCursorEnterCallback(window, enter_callback);
    // glfwSetScrollCallback(window,      scroll_callback);
    glfwSetWindowSizeCallback(window,  window_size_callback);

    // Setup ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfwGL3_Init(window, false);
    
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("../roboto.ttf", 20);

    // Koch_Snowflake_Scene scene = {};
    Triangulation_Scene scene = {};

    glfwSetWindowUserPointer(window, &scene);
        
    // Notify the scene what the width and height is
    scene_window_size_callback(&scene, width, height);

    // Programs main loop
    while (!glfwWindowShouldClose(window) && is_running) {
        ImGui_ImplGlfwGL3_NewFrame();

        update_and_render_scene(&scene);

        ImGui::Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();
    return 0;
}
