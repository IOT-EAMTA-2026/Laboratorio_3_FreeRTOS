#include "shared_types.h"

rgb_color_t g_current_color = {
    .r = 255,
    .g = 0,
    .b = 0
};

SemaphoreHandle_t g_color_mutex = NULL;

volatile uint32_t g_pending_timers = 0;