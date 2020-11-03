#define GLFW_INCLUDE_NONE
#include <cstdio>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"

#include "koch_snowflake.cpp"

void
opengl_debug_callback(GLenum source,
                      GLenum type,
                      GLenum id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message,
                      const void* user_params) {
    std::string msg("[OpenGL] ");
    switch (severity) {
        case GL_DEBUG_SEVERITY_LOW: {
            msg.append("<low severity> ");
        } break;

        case GL_DEBUG_SEVERITY_MEDIUM: {
            msg.append("<medium severity> ");
        } break;

        case GL_DEBUG_SEVERITY_HIGH: {
            msg.append("<high severity> ");
        } break;
    }

    msg.append(message);

    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            printf("Error: %s\n", msg.c_str());
            break;

        case GL_DEBUG_TYPE_PERFORMANCE:
            printf("Performance issue: %s\n", msg.c_str());
            break;

        default: break;
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
    GLFWwindow* window = glfwCreateWindow(640, 480, "Hello World", 0, 0);
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

    // Setup ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfwGL3_Init(window, false);
    
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("../roboto.ttf", 16);

    // Programs main loop
    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplGlfwGL3_NewFrame();

        update_and_render_scene();

        ImGui::Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();
    return 0;
}
