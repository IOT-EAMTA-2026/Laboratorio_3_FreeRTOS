#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "shared_types.h"

// Declaración externa de tu tarea de la terminal (TASK B)
extern void echo_task(void *arg);

static const char *MAIN_TAG = "MAIN_APP";

#define QUEUE_LENGTH 10

// --- TASK C (Simulada para pruebas de Hardware / LEDs) ---
void led_processor_task_C(void *pvParameters) {
    // Recuperamos el handle de la cola pasado desde el main
    QueueHandle_t xQueue = (QueueHandle_t)pvParameters;
    led_command_t comando;

    if (xQueue == NULL) {
        ESP_LOGE(MAIN_TAG, "[TASK C] Error: Cola inválida recibida.");
        vTaskDelete(NULL);
    }

    ESP_LOGI(MAIN_TAG, "[TASK C] Iniciada. Esperando comandos de la cola...");

    while (1) {
        // Bloqueo perpetuo hasta que entre un dato desde la TASK B
        if (xQueueReceive(xQueue, &comando, portMAX_DELAY) == pdTRUE) {
            ESP_LOGW(MAIN_TAG, "[TASK C] ¡Comando Recibido desde la cola!");
            ESP_LOGI(MAIN_TAG, "[TASK C] -> R:%d G:%d B:%d durante %d segundos.",
                     comando.color.r, comando.color.g, comando.color.b, comando.delay_s);
            
            // Aquí irá en el futuro tu lógica real para encender los LEDs físicos
        }
    }
}

// --- CONFIGURACIÓN PRINCIPAL ---
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(MAIN_TAG, "Inicializando aplicación...");

    // 1. Crear la cola de comandos con longitud 10 según requerimiento 5.3
    QueueHandle_t main_led_queue = xQueueCreate(QUEUE_LENGTH, sizeof(led_command_t));

    // Verificar que la memoria de la cola se asignó correctamente
    if (main_led_queue == NULL) {
        ESP_LOGE(MAIN_TAG, "Error crítico: No se pudo crear la cola de comandos.");
        return; 
    }

    // 2. Crear TASK B (Terminal UART) pasando el handle de la cola como parámetro (último argumento)
    xTaskCreate(echo_task, 
                "uart_echo_task", 
                4096, 
                (void *)main_led_queue, // <--- Inyección de la cola
                10, 
                NULL);

    // 3. Crear TASK C (Procesador LED de prueba) pasando el mismo handle de la cola
    xTaskCreate(led_processor_task_C, 
                "led_task_c", 
                2048, 
                (void *)main_led_queue, // <--- Inyección de la misma cola
                9, 
                NULL);
}