#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include "core.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct platform_window platform_window_t;

/* Key codes (platform-independent) */
typedef enum input_key {
    INPUT_KEY_W = 0,
    INPUT_KEY_A,
    INPUT_KEY_S,
    INPUT_KEY_D,
    INPUT_KEY_SPACE,
    INPUT_KEY_ESC,
    INPUT_KEY_SHIFT,
    INPUT_KEY_CTRL,
    INPUT_KEY_COUNT
} input_key_t;

/* Mouse buttons */
typedef enum input_mouse_button {
    INPUT_MOUSE_LEFT = 0,
    INPUT_MOUSE_RIGHT,
    INPUT_MOUSE_MIDDLE,
    INPUT_MOUSE_BUTTON_COUNT
} input_mouse_button_t;

/* Update (call every frame) */
void input_update(platform_window_t *window);

/* Keyboard state query */
bool input_key_pressed(input_key_t key);
bool input_key_just_pressed(input_key_t key);
bool input_key_just_released(input_key_t key);

/* Mouse state query */
bool input_mouse_button_pressed(input_mouse_button_t btn);
bool input_mouse_button_just_pressed(input_mouse_button_t btn);
void input_mouse_position(float *x, float *y);
void input_mouse_delta(float *dx, float *dy);

/* Mouse mode */
void input_set_mouse_captured(bool captured);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INPUT_H */
