#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

/*
 * Largo de la cola de comandos.
 * Este mismo valor se usa como cantidad maxima de timers pendientes.
 */
#define QUEUE_LENGTH 10

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
 */
typedef struct {
    rgb_color_t color;
    uint32_t delay_s;
} led_command_t;

/*
 * Color actual compartido.
 */
extern rgb_color_t g_current_color;

/*
 * Mutex que protege g_current_color.
 */
extern SemaphoreHandle_t g_color_mutex;

/*
 * Cola compartida entre TASK B y TASK C.
 */
extern QueueHandle_t led_queue;

/*
 * Cantidad de timers pendientes.
 * Se usa para responder al comando STATUS (opcional)
 */
extern volatile uint32_t g_pending_timers;

#endif