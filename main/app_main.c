#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "shared_types.h"
#include "rgb_led.h"
#include "task_b.h"
#include "task_c.h"
#include "task_a.h"

/*
   esp_log.h permite usar:
 * ESP_LOGI = mensaje informativo.
 * ESP_LOGW = warning, algo raro pero no fatal.
 * ESP_LOGE = error importante.
*/

static const char *MAIN_TAG = "MAIN_APP";

#define QUEUE_LENGTH 10

// --- CONFIGURACIÓN PRINCIPAL ---
void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Inicializando aplicación...");

    // Crear la cola de comandos con longitud 10 según requerimiento 5.3
    led_queue = xQueueCreate(QUEUE_LENGTH, sizeof(led_command_t));
    if (led_queue == NULL) {
        ESP_LOGE(MAIN_TAG, "Error crítico: No se pudo crear la cola.");
        return;
    }

    // Verificar que la memoria de la cola se asignó correctamente
    if (led_queue == NULL) {
        ESP_LOGE(MAIN_TAG, "Error crítico: No se pudo crear la cola de comandos.");
        return; 
    }
     /*
     * Color inicial.
     * TASK A va a parpadear inicialmente en rojo.
     */
    g_current_color.r = 255;
    g_current_color.g = 0;
    g_current_color.b = 0;

    /*
     * Cantidad inicial de timers pendientes.para opcional
     */
    g_pending_timers = 0;

    /*
     * Mutex que protege el color global (g_current_color).
     */
    g_color_mutex = xSemaphoreCreateMutex();

    if (g_color_mutex == NULL) {
        ESP_LOGE(TAG, "No se pudo crear el mutex del color");
        return;
    }

    /*
     * Inicialización del LED RGB.
     */
    rgb_led_init();

    ESP_LOGI(TAG, "LED RGB inicializado");
    /*
     * TASK A: menor prioridad.
     * TASK C: prioridad intermedia.
     * TASK B: mayor prioridad.
     */
        BaseType_t task_created; // dijimos que es un tipo base de FreeRTOS que sustituye nuestro int 32 o int 16 segun el micro.
    /*
     * xTaskCreate() crea una tarea.
     *
     * ejemplo de parametros:
     * 1) task_a: función que ejecuta la tarea.
     * 2) "task_a": nombre de la tarea para el debug.
     * 3) 4096: stack asignado a la tarea.
     * 4) NULL: parámetro que recibe la tarea. En este caso nada.
     * 5) tskIDLE_PRIORITY + 1: prioridad baja.
     * 6) NULL: handle de la tarea. Aca no se necesita.
     *
     * tskIDLE_PRIORITY es la prioridad mínima del sistema que de default limpia la memoria de las tareas borradas.
     * Al poner +1, TASK A queda apenas arriba de la tarea idle.
     */

    /*
 * TASK A: menor prioridad.Parpadeo del led.
 */

    task_created = xTaskCreate(
        task_a,
        "task_a",
        4096,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear TASK A");
        return;
    }


    // Crea TASK B (Terminal UART) pasando el handle de la cola como parámetro (último argumento)
    task_created = xTaskCreate(
        task_b  ,
        "task_b",
        4096,
        (void *)led_queue,
        10,
        NULL
    );  

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear TASK B");
        return;
    }

//Crea TASK C (Procesador LED de prueba) pasando el mismo handle de la cola
    task_created = xTaskCreate(
        task_c,
        "led_task",
        4096,
        (void *)led_queue,
        10,
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear TASK C");
        return;
    }
    ESP_LOGI(TAG, "Todas las tareas fueron creadas correctamente");
}