#include <stdint.h>
#ifndef SHARED_TYPES
#define SHARED_TYPES
// Color del LED
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;
// Comando recibido por UART
typedef struct {
    rgb_color_t color; // color a aplicar
    uint32_t delay_s; // segundos de espera antes de aplicar
} led_command_t;
#endif 
