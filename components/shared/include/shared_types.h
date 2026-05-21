#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/*
 * Color del LED RGB.
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

/*
 * Comando recibido por UART.
 *
 * color: color final deseado.
 * delay_s: tiempo en segundos.
 *
 * En esta versión extendida:
 * - delay_s se usa como espera antes de iniciar el cambio.
 * - También se usa como duración de la transición gradual.
 */
typedef struct {
    rgb_color_t color;
    uint32_t delay_s;
} led_command_t;

/*
 * Color actual compartido.
 * TASK A lo lee.
 * TASK C y la tarea de transición lo modifican.
 */
extern rgb_color_t g_current_color;

/*
 * Mutex que protege g_current_color.
 */
extern SemaphoreHandle_t g_color_mutex;

/*
 * Cantidad de timers pendientes.
 * Se usa para responder al comando STATUS.
 */
extern volatile uint32_t g_pending_timers;

#endif