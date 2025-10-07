#if !defined(ANDROID) && !defined(__ANDROID__) && !defined(MARU_PLATFORM_IOS)

#include "input.h"
#include "window.h"

#include <string.h>
#include <GLFW/glfw3.h>

/* Internal state */
static uint8_t s_key_state[INPUT_KEY_COUNT];
static uint8_t s_key_prev[INPUT_KEY_COUNT];
static uint8_t s_mouse_state[INPUT_MOUSE_BUTTON_COUNT];
static uint8_t s_mouse_prev[INPUT_MOUSE_BUTTON_COUNT];
static float s_mouse_x = 0.0f;
static float s_mouse_y = 0.0f;
static float s_mouse_prev_x = 0.0f;
static float s_mouse_prev_y = 0.0f;
static bool s_mouse_captured = false;

/* Key mapping table: input_key_t -> GLFW key code */
static const int s_key_map[INPUT_KEY_COUNT] = {
    [INPUT_KEY_W] = GLFW_KEY_W,
    [INPUT_KEY_A] = GLFW_KEY_A,
    [INPUT_KEY_S] = GLFW_KEY_S,
    [INPUT_KEY_D] = GLFW_KEY_D,
    [INPUT_KEY_SPACE] = GLFW_KEY_SPACE,
    [INPUT_KEY_ESC] = GLFW_KEY_ESCAPE,
    [INPUT_KEY_SHIFT] = GLFW_KEY_LEFT_SHIFT,
    [INPUT_KEY_CTRL] = GLFW_KEY_LEFT_CONTROL,
};

/* Mouse button mapping */
static const int s_mouse_map[INPUT_MOUSE_BUTTON_COUNT] = {
    [INPUT_MOUSE_LEFT] = GLFW_MOUSE_BUTTON_LEFT,
    [INPUT_MOUSE_RIGHT] = GLFW_MOUSE_BUTTON_RIGHT,
    [INPUT_MOUSE_MIDDLE] = GLFW_MOUSE_BUTTON_MIDDLE,
};

void input_update(platform_window_t *window) {
    if (!window || !window->impl) return;

    GLFWwindow *glfw = (GLFWwindow*) window->impl;

    /* Save previous frame state */
    memcpy(s_key_prev, s_key_state, sizeof(s_key_state));
    memcpy(s_mouse_prev, s_mouse_state, sizeof(s_mouse_state));
    s_mouse_prev_x = s_mouse_x;
    s_mouse_prev_y = s_mouse_y;

    /* Update keyboard state */
    for (int i = 0; i < INPUT_KEY_COUNT; i++) {
        int glfw_key = s_key_map[i];
        s_key_state[i] = (glfwGetKey(glfw, glfw_key) == GLFW_PRESS) ? 1 : 0;
    }

    /* Update mouse button state */
    for (int i = 0; i < INPUT_MOUSE_BUTTON_COUNT; i++) {
        int glfw_btn = s_mouse_map[i];
        s_mouse_state[i] = (glfwGetMouseButton(glfw, glfw_btn) == GLFW_PRESS) ? 1 : 0;
    }

    /* Update mouse position */
    double mx, my;
    glfwGetCursorPos(glfw, &mx, &my);
    s_mouse_x = (float) mx;
    s_mouse_y = (float) my;
}

bool input_key_pressed(input_key_t key) {
    if (key < 0 || key >= INPUT_KEY_COUNT) return false;
    return s_key_state[key] != 0;
}

bool input_key_just_pressed(input_key_t key) {
    if (key < 0 || key >= INPUT_KEY_COUNT) return false;
    return s_key_state[key] && !s_key_prev[key];
}

bool input_key_just_released(input_key_t key) {
    if (key < 0 || key >= INPUT_KEY_COUNT) return false;
    return !s_key_state[key] && s_key_prev[key];
}

bool input_mouse_button_pressed(input_mouse_button_t btn) {
    if (btn < 0 || btn >= INPUT_MOUSE_BUTTON_COUNT) return false;
    return s_mouse_state[btn] != 0;
}

bool input_mouse_button_just_pressed(input_mouse_button_t btn) {
    if (btn < 0 || btn >= INPUT_MOUSE_BUTTON_COUNT) return false;
    return s_mouse_state[btn] && !s_mouse_prev[btn];
}

void input_mouse_position(float *x, float *y) {
    if (x) *x = s_mouse_x;
    if (y) *y = s_mouse_y;
}

void input_mouse_delta(float *dx, float *dy) {
    if (dx) *dx = s_mouse_x - s_mouse_prev_x;
    if (dy) *dy = s_mouse_y - s_mouse_prev_y;
}

void input_set_mouse_captured(bool captured) {
    s_mouse_captured = captured;
    /* Note: actual GLFW cursor mode will be set in input_update if needed,
     * or we can set it here if we store the window pointer */
}

#endif
