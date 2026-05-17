#ifndef TOUCHPAD_H
#define TOUCHPAD_H

#include <stdint.h>
#include <stdbool.h>

#define TOUCHPAD_BUTTON_COUNT 6

void touchpad_init(void);
bool touchpad_is_pressed(uint8_t button_index);

#endif