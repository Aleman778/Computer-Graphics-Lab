#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <cassert>

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <vector>
#include <random>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <deque>

#include <glm.hpp>
#include <gtx/hash.hpp>
#include <gtc/constants.hpp>
#include <gtx/string_cast.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/matrix_transform_2d.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>

#include <GL/glew.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define array_count(array) (sizeof(array)/sizeof((array)[0]))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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

static const glm::vec4 red_color          = glm::vec4(1.0f,  0.4f,  0.4f,  1.0f);
static const glm::vec4 green_color        = glm::vec4(0.4f,  1.0f,  0.4f,  1.0f);
static const glm::vec4 blue_color         = glm::vec4(0.3f,  0.3f,  1.0f,  1.0f);
static const glm::vec4 magenta_color      = glm::vec4(0.9f,  0.3f,  1.0f,  1.0f);
static const glm::vec4 primary_bg_color   = glm::vec4(0.35f, 0.35f, 0.37f, 1.0f);
static const glm::vec4 primary_fg_color   = glm::vec4(0.3f,  0.5f,  0.8f,  0.0f);
static const glm::vec4 secondary_fg_color = glm::vec4(0.46f, 0.72f, 1.0f,  0.0f);

static constexpr f32 quarter_pi = glm::pi<f32>()/4.0f;
static constexpr f32 half_pi    = glm::pi<f32>()/2.0f;
static constexpr f32 pi         = glm::pi<f32>();
static constexpr f32 two_pi     = glm::pi<f32>()*2.0f;

static bool is_running = true;

enum Scene_Type {
    Scene_Koch_Snowflake,
    Scene_Triangulation,
    Scene_Basic_3D_Graphics,
    Scene_Simple_World,
};

struct Button_State {
    int half_transition_count;
    bool ended_down;
};

struct Input {
    f32 mouse_x;
    f32 mouse_y;

    f32 mouse_scroll_x;
    f32 mouse_scroll_y;

    bool mouse_locked;
    
    Button_State left_mb;
    Button_State right_mb;
    Button_State middle_mb;

    Button_State w_key;
    Button_State a_key;
    Button_State s_key;
    Button_State d_key;
    Button_State e_key;
    Button_State c_key;

    Button_State alt_key;
    Button_State shift_key;
    Button_State control_key;
    Button_State escape_key;
};

struct Window {
    i32 width;
    i32 height;
    Input input;
    bool is_focused;
};

#include "renderer.h"
#include "ecs.h"

bool was_pressed(Button_State* state);

void opengl_debug_callback(GLenum source,
                           GLenum type,
                           GLenum id,
                           GLenum severity,
                           GLsizei length,
                           const GLchar* message,
                           const void* user_params);

std::string read_entire_file_to_string(std::string filepath);

const char* find_resource_folder();

// NOTE(alexander): the path to the resource folder e.g. res/
static const char* res_folder = find_resource_folder();
