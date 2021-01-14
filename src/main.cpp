#include "main.h"
#include "perlin_noise.cpp"
#include "geometry.cpp"
#include "hdr_loader.cpp"
#include "renderer.cpp"
#include "ecs.cpp"
#include "koch_snowflake.cpp"    // Lab 1
#include "triangulation.cpp"     // Lab 2
#include "basic_3d_graphics.cpp" // Lab 3
#include "simple_world.cpp"      // Lab 4
#include "world_editor.cpp"      // Lab 4

bool
was_pressed(Button_State* state) {
    return state->half_transition_count > 1 ||
        (state->half_transition_count == 1 && state->ended_down);
}

static void
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

static void
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

        case GLFW_KEY_ESCAPE: {
            state = &window->input.escape_key;
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

static void
window_mouse_callback(GLFWwindow* glfw_window, int button, int action, int mods) {
    if (ImGui::IsAnyWindowHovered()) {
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

static void
window_scroll_callback(GLFWwindow* glfw_window, double xoffset, double yoffset) {
    if (ImGui::IsAnyWindowHovered()) {
        return;
    }

    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        window->input.mouse_scroll_x = (f32) xoffset;
        window->input.mouse_scroll_y = (f32) yoffset;
    }
}

static void
window_cursor_pos_callback(GLFWwindow* glfw_window, double xpos, double ypos) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        if (window->input.mouse_locked) {
            if (window->is_focused) {
                window->input.mouse_delta_x += (f32) xpos - (f32) (window->width/2);
                window->input.mouse_delta_y += (f32) ypos - (f32) (window->height/2);
            }
        } else {
            window->input.mouse_delta_x += (f32) xpos - window->input.mouse_x;
            window->input.mouse_delta_y += (f32) ypos - window->input.mouse_y;
            window->input.mouse_x = (f32) xpos;
            window->input.mouse_y = (f32) ypos;
        }
    }
}

static void
window_focus_callback(GLFWwindow* glfw_window, int focused) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        window->is_focused = focused == GLFW_TRUE;
    }
}

static void
window_size_callback(GLFWwindow* glfw_window, i32 width, i32 height) {
    Window* window = (Window*) glfwGetWindowUserPointer(glfw_window);
    if (window) {
        window->width = width;
        window->height = height;
    }
}

// LINK: https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
std::string
read_entire_file_to_string(std::string filepath) {
    std::ifstream t(filepath);
    std::string str;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
               std::istreambuf_iterator<char>());
    return str;
}

static double
get_time() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>
        (std::chrono::high_resolution_clock::now() - global_time_epoch).count() / 1000000000.0;
}

int
main() {
    if (!glfwInit()) {
        return -1;
    }

    
#ifdef __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif


    // Setup time
    global_time_epoch = std::chrono::high_resolution_clock::now();

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
    window.is_focused = true;

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
    glfwSetWindowFocusCallback(glfw_window, window_focus_callback);
    glfwSetScrollCallback(glfw_window,      window_scroll_callback);
    glfwSetWindowSizeCallback(glfw_window,  window_size_callback);

    auto koch_snowflake_scene = new Koch_Snowflake_Scene();
    auto triangulation_scene = new Triangulation_Scene();
    auto basic_3d_graphics_scene = new Basic_3D_Graphics_Scene();
    auto simple_world_scene = new Simple_World_Scene();
    auto world_editor = new World_Editor();
    world_editor->world = &simple_world_scene->world;
    Scene_Type current_scene_type = Scene_Simple_World;

    // Setup ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImGuiIO& io = ImGui::GetIO();
    std::ostringstream path_stream;
    path_stream << res_folder;
    path_stream << "fonts/roboto.ttf";
    std::string filepath = path_stream.str();
    io.Fonts->AddFontFromFileTTF(filepath.c_str(), 18.0f);

    // Vsync
    glfwSwapInterval(1);

    // Set target frame time based on monitors refresh rate if possible
    float target_frame_time = 1.0f/60.0f;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        target_frame_time = 1.0f/(f32) mode->refreshRate;
    }

    // Main program loop
    u32 fps = 0;
    u32 fps_counter = 0;
    bool show_performance_gui = false;
    double last_time = get_time();
    double fps_timer = 0.0;
    double update_timer = 0.0;
    while (!glfwWindowShouldClose(glfw_window) && is_running) {
        double curr_time = get_time();
        double delta_time = curr_time - last_time;
        last_time = curr_time;
        fps_timer += delta_time;
        update_timer += delta_time;

        if (fps_timer >= 1.0) {
            fps_timer = 0;
            fps = fps_counter;
            fps_counter = 0;
        }

        
        if (window.input.mouse_locked) {
            glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        }

        bool should_render = false;
        while (update_timer >= target_frame_time) {
            switch (current_scene_type) {
                case Scene_Koch_Snowflake: {
                    update_scene(koch_snowflake_scene, &window, target_frame_time);
                } break;

                case Scene_Triangulation: {
                    update_scene(triangulation_scene, &window, target_frame_time);
                } break;

                case Scene_Basic_3D_Graphics: {
                    if (!basic_3d_graphics_scene->is_initialized) {
                        if (!initialize_scene(basic_3d_graphics_scene, &window)) {
                            is_running = false;
                            return 1;
                        }
                    }

                    update_systems(&basic_3d_graphics_scene->world, basic_3d_graphics_scene->main_systems, target_frame_time);
                } break;

                case Scene_Simple_World: {
                    if (!simple_world_scene->is_initialized) {
                        if (!initialize_scene(simple_world_scene, &window)) {
                            is_running = false;
                            return 1;
                        }
                    }
                    update_systems(&simple_world_scene->world, simple_world_scene->main_systems, target_frame_time);
                } break;

                case Scene_World_Editor: {
                    update_world_editor(world_editor, &window, target_frame_time);
                } break;
            }

            // Update the timer
            update_timer -= target_frame_time;
            should_render = true;
        }

        // Render
        if (should_render) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            // Render current scene
            switch (current_scene_type) {
                case Scene_Koch_Snowflake: {
                    render_scene(koch_snowflake_scene, &window, target_frame_time);
                } break;

                case Scene_Triangulation: {
                    render_scene(triangulation_scene, &window, target_frame_time);
                } break;

                case Scene_Basic_3D_Graphics: {
                    render_scene(basic_3d_graphics_scene, &window, target_frame_time);
                } break;

                case Scene_Simple_World: {
                    render_scene(simple_world_scene, &window, target_frame_time);
                } break;

                case Scene_World_Editor: {
                    render_world_editor(world_editor, &window, target_frame_time);
                } break;

                default: {
                    // NOTE(alexander): invalid scene, just render background
                    glClearColor(primary_bg_color.x, primary_bg_color.y, primary_bg_color.z, primary_bg_color.w);
                    glClear(GL_COLOR_BUFFER_BIT);
                } break;
            }

            static bool show_performance = true;
            ImGui::Begin("Performance", &show_performance);
            ImGui::Text("FPS: %u", fps);
            ImGui::End();

            // Menu bar for switching between scenes
            if (ImGui::BeginMainMenuBar()) {
                static bool labs_enabled = true;
                if (ImGui::BeginMenu("Labs", labs_enabled)) {
                    if (ImGui::MenuItem("Lab 1 - Koch Snowflake")) current_scene_type = Scene_Koch_Snowflake;
                    if (ImGui::MenuItem("Lab 2 - Triangulation")) current_scene_type = Scene_Triangulation;
                    if (ImGui::MenuItem("Lab 3 - Basic 3D Graphics")) current_scene_type = Scene_Basic_3D_Graphics;
                    if (ImGui::MenuItem("Lab 4 - Simple World")) current_scene_type = Scene_Simple_World;
                    if (ImGui::MenuItem("Lab 4 - World Editor")) current_scene_type = Scene_World_Editor;
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            if (!window.input.mouse_locked) {
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            } else {
                ImGui::EndFrame();
            }

            // Reset input state
            window.input.mouse_delta_x = 0.0f;
            window.input.mouse_delta_y = 0.0f;
            window.input.mouse_scroll_x = 0.0f;
            window.input.mouse_scroll_y = 0.0f;
            window.input.left_mb.half_transition_count = 0;
            window.input.right_mb.half_transition_count = 0;
            window.input.middle_mb.half_transition_count = 0;
            window.input.alt_key.half_transition_count = 0;
            window.input.shift_key.half_transition_count = 0;
            window.input.control_key.half_transition_count = 0;
            window.input.escape_key.half_transition_count = 0;
            window.input.w_key.half_transition_count = 0;
            window.input.a_key.half_transition_count = 0;
            window.input.s_key.half_transition_count = 0;
            window.input.d_key.half_transition_count = 0;
            window.input.e_key.half_transition_count = 0;
            window.input.c_key.half_transition_count = 0;

            // handle mouse locking
            if (window.input.mouse_locked) {
                glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                if (window.is_focused) {
                    glfwSetCursorPos(glfw_window, window.width/2, window.height/2);
                }
            } else {
                glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            
            glfwSwapBuffers(glfw_window);
            glfwPollEvents();
            fps_counter++;

            if (window.input.mouse_locked && !window.is_focused) {
                window.input.mouse_x = (f32) (window.width/2);
                window.input.mouse_y = (f32) (window.height/2);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(1));
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfw_window);
    glfwTerminate();
    return 0;
}

// NOTE(alexander): don't expose this to the rest of the codebase!
#include <sys/types.h>
#include <sys/stat.h>

const char*
find_resource_folder() {
    struct stat info;

    if (stat("res", &info) == 0 && info.st_mode & S_IFDIR) {
        return "res/";
    }

    if (stat("../res", &info) == 0 && info.st_mode & S_IFDIR) {
        return "../res/";
    }

    assert(0 && "Failed to find res folder!");
    return "";
}
