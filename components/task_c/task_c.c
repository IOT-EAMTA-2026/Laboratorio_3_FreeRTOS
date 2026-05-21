#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "shared_types.h"

// variables declaradas en main
extern QueueHandle_t led_queue;
extern SemaphoreHandle_t g_color_mutex;
extern rgb_color_t g_current_color;

// Callback del timer one-shot
static void timer_callback(TimerHandle_t xTimer)
{
    // Recuperar el puntero al color guardado en pvTimerID
    rgb_color_t *color = (rgb_color_t *) pvTimerGetTimerID(xTimer);

    // Tomar el mutex y actualizar el color compartido
    if (xSemaphoreTake(g_color_mutex, portMAX_DELAY) == pdTRUE) {
        g_current_color = *color;
        xSemaphoreGive(g_color_mutex);
    }

    // Liberar la memoria dinámica
    vPortFree(color);

    // Eliminar el timer para evitar fugas de recursos
    xTimerDelete(xTimer, 0);
}

// Tarea C: gestión de timers
void task_c(void *pvParameters)
{
    led_command_t command;

    while (1) {
        // Bloquearse esperando un comando en la cola (sin timeout)
        if (xQueueReceive(led_queue, &command, portMAX_DELAY) == pdTRUE) {

            // Asignar memoria dinámica para el color
            rgb_color_t *color = (rgb_color_t *) pvPortMalloc(sizeof(rgb_color_t));
            if (color == NULL) {
                // Si no hay memoria, descartar el comando
                continue;
            }

            // Copiar el color del comando al bloque recién asignado
            *color = command.color;

            // Calcular el período del timer
            TickType_t period = command.delay_s * 1000 / portTICK_PERIOD_MS;

            // Crear el timer one-shot con pvTimerID apuntando al color
            TimerHandle_t timer = xTimerCreate(
                "led_timer",        // Nombre
                period,             // Período
                pdFALSE,            // one-shot (no auto-reload)
                (void *) color,     // pvTimerID = puntero al color
                timer_callback      // Callback
            );

            if (timer == NULL) {
                // Si no se pudo crear el timer, liberar la memoria
                vPortFree(color);
                continue;
            }

            // Arrancar el timer
            xTimerStart(timer, 0);
        }
    }
}