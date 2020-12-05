#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <cassert>

#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <random>
#include <functional>
#include <unordered_set>
#include <unordered_map>

#include <glm.hpp>
#include <gtx/hash.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/matrix_transform_2d.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>

#include <GL/glew.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"

#define array_count(array) (sizeof(array)/sizeof((array)[0]))

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

static bool is_running = true;

enum Scene_Type {
    Scene_Koch_Snowflake,
    Scene_Triangulation,
    Scene_Basic_3D_Graphics,
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
};

struct Window {
    i32 width;
    i32 height;
    Input input;
};

#include "renderer.h"

bool was_pressed(Button_State* state);

void opengl_debug_callback(GLenum source,
                           GLenum type,
                           GLenum id,
                           GLenum severity,
                           GLsizei length,
                           const GLchar* message,
                           const void* user_params);
