#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "shared_types.h"

extern void echo_task(void *arg);
extern void task_c(void *pvParameters);

static const char *MAIN_TAG = "MAIN_APP";

#define QUEUE_LENGTH 10

// Variables globales compartidas
QueueHandle_t led_queue;
SemaphoreHandle_t led_mutex;
rgb_color_t current_color = {0, 0, 0};

// --- CONFIGURACIÓN PRINCIPAL ---
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(MAIN_TAG, "Inicializando aplicación...");

    // 1. Crear la cola de comandos con longitud 10 según requerimiento 5.3
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

    // Crear mutex
    led_mutex = xSemaphoreCreateMutex();
    if (led_mutex == NULL) {
        ESP_LOGE(MAIN_TAG, "Error crítico: No se pudo crear el mutex.");
        return;
    }

    // 2. Crear TASK B (Terminal UART) pasando el handle de la cola como parámetro (último argumento)
    xTaskCreate(echo_task, 
                "uart_echo_task", 
                4096, 
                (void *)led_queue, // <--- Inyección de la cola
                10, 
                NULL);

    // 3. Crear TASK C (Procesador LED de prueba) pasando el mismo handle de la cola
    xTaskCreate(task_c, 
                "led_task", 
                4096, 
                (void *)led_queue, // <--- Inyección de la cola
                10, 
                NULL);

}