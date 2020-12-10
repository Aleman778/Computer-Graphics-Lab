#include "main.h"
#include "renderer.cpp"
#include "koch_snowflake.cpp"
#include "triangulation.cpp"
#include "basic_3d_graphics.cpp"

bool
was_pressed(Button_State* state) {
    return state->half_transition_count > 1 ||
        (state->half_transition_count == 1 && state->ended_down);
}

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
window_key_callback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods) {
    if (action == GLFW_REPEAT) return; // NOTE(alexander): don't care about repeat, breaks my input system!
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    Button_State* state = NULL;
    
    switch (key) {
        case GLFW_KEY_LEFT_ALT: 
        case GLFW_KEY_RIGHT_ALT: {
            state = &window->input.alt_key;
        } break;
            
        case GLFW_KEY_LEFT_SHIFT: 
        case GLFW_KEY_RIGHT_SHIFT: {
            state = &window->input.shift_key;
        } break;
            
        case GLFW_KEY_LEFT_CONTROL: 
        case GLFW_KEY_RIGHT_CONTROL: {
            state = &window->input.control_key;
        } break;

        case GLFW_KEY_W: {
            state = &window->input.w_key;
        } break;
 
        case GLFW_KEY_A: {
            state = &window->input.a_key;
        } break;
 
        case GLFW_KEY_S: {
            state = &window->input.s_key;
        } break;
 
        case GLFW_KEY_D: {
            state = &window->input.d_key;
        } break;
 
        case GLFW_KEY_E: {
            state = &window->input.e_key;
        } break;
 
        case GLFW_KEY_C: {
            state = &window->input.c_key;
        } break;
    }
    
    if (state) {
        state->ended_down = action == GLFW_PRESS || action == GLFW_REPEAT;
        state->half_transition_count++;
    }
}

void
window_mouse_callback(GLFWwindow* glfw_window, int button, int action, int mods) {
    if (ImGui::IsMouseHoveringAnyWindow()) {
        ImGui_ImplGlfwGL3_MouseButtonCallback(glfw_window, button, action, mods);
        return;
    }

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
    if (ImGui::IsMouseHoveringAnyWindow()) {
        ImGui_ImplGlfwGL3_ScrollCallback(glfw_window, xoffset, yoffset);
        return;
    }
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
    glfwSetKeyCallback(glfw_window,         window_key_callback);
    glfwSetMouseButtonCallback(glfw_window, window_mouse_callback);
    glfwSetCursorPosCallback(glfw_window,   window_cursor_pos_callback);
    // glfwSetCursorEnterCallback(glfw_window, enter_callback);
    glfwSetScrollCallback(glfw_window,      window_scroll_callback);
    glfwSetWindowSizeCallback(glfw_window,  window_size_callback);

    auto koch_snowflake_scene = new Koch_Snowflake_Scene();
    auto triangulation_scene = new Triangulation_Scene();
    auto basic_3d_graphics_scene = new Basic_3D_Graphics_Scene(); // may be big 
    Scene_Type current_scene_type = Scene_Basic_3D_Graphics;

    // Setup ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfwGL3_Init(glfw_window, false);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("../roboto.ttf", 18.0f);

    // Vsync
    glfwSwapInterval(1);

    // Programs main loop
    while (!glfwWindowShouldClose(glfw_window) && is_running) {
        ImGui_ImplGlfwGL3_NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            static bool labs_enabled = true;
            if (ImGui::BeginMenu("Labs", labs_enabled)) {
                if (ImGui::MenuItem("Lab 1 - Koch Snowflake")) current_scene_type = Scene_Koch_Snowflake;
                if (ImGui::MenuItem("Lab 2 - Triangulation")) current_scene_type = Scene_Triangulation;
                if (ImGui::MenuItem("Lab 3 - Basic 3D Graphics")) current_scene_type = Scene_Basic_3D_Graphics;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();

            switch (current_scene_type) {
                case Scene_Koch_Snowflake: {
                    update_and_render_scene(koch_snowflake_scene, &window);
                } break;
                    
                case Scene_Triangulation: {
                    update_and_render_scene(triangulation_scene, &window);
                } break;

                case Scene_Basic_3D_Graphics: {
                    update_and_render_scene(basic_3d_graphics_scene, &window);
                } break;

                default: { // NOTE(alexander): invalid scene, just render background
                    glClearColor(primary_bg_color.x, primary_bg_color.y, primary_bg_color.z, primary_bg_color.w);
                    glClear(GL_COLOR_BUFFER_BIT);
                } break;
            }
        }

        
        ImGui::Render();

        // reset input states and mouse scroll
        window.input.mouse_scroll_x = 0;
        window.input.mouse_scroll_y = 0;
        window.input.left_mb.half_transition_count = 0;
        window.input.right_mb.half_transition_count = 0;
        window.input.middle_mb.half_transition_count = 0;
        window.input.alt_key.half_transition_count = 0;
        window.input.shift_key.half_transition_count = 0;
        window.input.control_key.half_transition_count = 0;
        window.input.w_key.half_transition_count = 0;
        window.input.a_key.half_transition_count = 0;
        window.input.s_key.half_transition_count = 0;
        window.input.d_key.half_transition_count = 0;
        window.input.e_key.half_transition_count = 0;
        window.input.c_key.half_transition_count = 0;
        
        glfwSwapBuffers(glfw_window);
        glfwPollEvents();
    }

    glfwDestroyWindow(glfw_window);
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();
    return 0;
}
