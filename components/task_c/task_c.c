#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "shared_types.h"
#include "task_c.h"

static const char *TAG = "TASK_C";

typedef struct {
    bool active;
    rgb_color_t color;
    TimerHandle_t timer;
    rgb_color_t *color_ptr;
} pending_timer_t;

/*
 * Usa el mismo largo que la cola.
 * como QUEUE_LENGTH vale 10, puede registrar hasta 10 timers pendientes.
 */
static pending_timer_t pending_timers[QUEUE_LENGTH];

static bool same_color(rgb_color_t a, rgb_color_t b)
{
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
}

static void remove_pending_timer(TimerHandle_t timer)
{
    for (int i = 0; i < QUEUE_LENGTH; i++) {
        if (pending_timers[i].active && pending_timers[i].timer == timer) {
            pending_timers[i].active = false;
            pending_timers[i].timer = NULL;
            pending_timers[i].color_ptr = NULL;

            if (g_pending_timers > 0) {
                g_pending_timers--;
            }

            return;
        }
    }
}

static bool register_pending_timer(rgb_color_t color, TimerHandle_t timer, rgb_color_t *color_ptr)
{
    for (int i = 0; i < QUEUE_LENGTH; i++) {
        if (!pending_timers[i].active) {
            pending_timers[i].active = true;
            pending_timers[i].color = color;
            pending_timers[i].timer = timer;
            pending_timers[i].color_ptr = color_ptr;

            g_pending_timers++;

            return true;
        }
    }

    return false;
}

/*
 * Cancela solo UN timer previo del mismo color.
 *
 * Ejemplo:
 * ROJO 10
 * ROJO 4
 *
 * Cuando llega ROJO 4, se cancela el timer ROJO 10.
 * Hace return apenas cancela uno, no borra todos.
 */
static void cancel_previous_timer_same_color(rgb_color_t color)
{
    for (int i = 0; i < QUEUE_LENGTH; i++) {
        if (pending_timers[i].active && same_color(pending_timers[i].color, color)) {
            ESP_LOGW(
                TAG,
                "Cancelando timer previo del mismo color R=%u G=%u B=%u",
                color.r,
                color.g,
                color.b
            );

            xTimerStop(pending_timers[i].timer, 0);
            xTimerDelete(pending_timers[i].timer, 0);

            /*
             * Como el timer fue cancelado, su callback ya no va a liberar
             * esta memoria. Por eso se libera aca.
             */
            if (pending_timers[i].color_ptr != NULL) {
                vPortFree(pending_timers[i].color_ptr);
            }

            pending_timers[i].active = false;
            pending_timers[i].timer = NULL;
            pending_timers[i].color_ptr = NULL;

            if (g_pending_timers > 0) {
                g_pending_timers--;
            }

            return;
        }
    }
}

// Callback del timer one-shot.
static void timer_callback(TimerHandle_t xTimer)
{
    // Recuperar el puntero al color guardado en pvTimerID.
    rgb_color_t *color = (rgb_color_t *)pvTimerGetTimerID(xTimer);

    /*
     * El timer ya vencio, entonces deja de estar pendiente.
     */
    remove_pending_timer(xTimer);

    // Tomar el mutex y actualizar el color compartido.
    if (xSemaphoreTake(g_color_mutex, portMAX_DELAY) == pdTRUE) {
        g_current_color = *color;
        xSemaphoreGive(g_color_mutex);

        ESP_LOGI(
            TAG,
            "Color aplicado R:%u G:%u B:%u",
            color->r,
            color->g,
            color->b
        );
    }

    // Liberar la memoria dinamica.
    vPortFree(color);

    // Eliminar el timer para evitar fugas de recursos.
    xTimerDelete(xTimer, 0);
}

// Tarea C: gestion de timers.
void task_c(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    led_command_t command;

    uint32_t cycle_counter = 0;

    memset(pending_timers, 0, sizeof(pending_timers));

    while (1) {
        // Bloquearse esperando un comando en la cola.
        if (xQueueReceive(queue, &command, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(
                TAG,
                "Comando recibido R:%u G:%u B:%u | Delay:%lu s",
                command.color.r,
                command.color.g,
                command.color.b,
                (unsigned long)command.delay_s
            );

            cycle_counter++;

            if (cycle_counter >= 5) {
                cycle_counter = 0;

                UBaseType_t stack_free = uxTaskGetStackHighWaterMark(NULL);

                ESP_LOGI(
                    TAG,
                    "Stack minimo libre TASK C: %u words",
                    (unsigned int)stack_free
                );
            }

            /*
             * Opcional:
             * Si ya habia un timer pendiente con este mismo color,
             * se cancela solo ese timer previo.
             */
            cancel_previous_timer_same_color(command.color);

            // Asignar memoria dinamica para el color.
            rgb_color_t *color = (rgb_color_t *)pvPortMalloc(sizeof(rgb_color_t));

            if (color == NULL) {
                ESP_LOGE(TAG, "No hay memoria para guardar el color");
                continue;
            }

            // Copiar el color del comando al bloque recién asignado.
            *color = command.color;

            // Calcular el periodo del timer.
            TickType_t period = pdMS_TO_TICKS(command.delay_s * 1000);

            if (period == 0) {
                period = 1;
            }

            // Crear el timer one-shot con pvTimerID apuntando al color.
            TimerHandle_t timer = xTimerCreate(
                "led_timer",
                period,
                pdFALSE,
                (void *)color,
                timer_callback
            );

            if (timer == NULL) {
                ESP_LOGE(TAG, "No se pudo crear el timer");
                vPortFree(color);
                continue;
            }

            /*
             * Registrar el timer como pendiente.
             * Esto permite cancelarlo si llega otro comando del mismo color.
             */
            if (!register_pending_timer(command.color, timer, color)) {
                ESP_LOGW(TAG, "Tabla de timers pendientes llena");
                xTimerDelete(timer, 0);
                vPortFree(color);
                continue;
            }

            // Arrancar el timer.
            if (xTimerStart(timer, 0) != pdPASS) {
                ESP_LOGE(TAG, "No se pudo iniciar el timer");

                remove_pending_timer(timer);
                xTimerDelete(timer, 0);
                vPortFree(color);

                continue;
            }

            ESP_LOGI(
                TAG,
                "Timer creado. Timers pendientes=%lu",
                (unsigned long)g_pending_timers
            );
        }
    }
}
