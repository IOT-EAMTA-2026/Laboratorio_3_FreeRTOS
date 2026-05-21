#include "task_a.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "shared_types.h"
#include "rgb_led.h"

static const char *TAG = "TASK_A";

#define BLINK_ON_TIME_MS   500
#define BLINK_OFF_TIME_MS  500

void task_a(void *pvParameters)
{
    (void)pvParameters;//Se hace el cast a void para evitar warnings del compilador por parametro no utilizado.
    //void para mostrar que se ignora intencionalmente

    ESP_LOGI(TAG, "TASK A iniciada");

    uint32_t cycle_counter = 0;//cuenta ciclos para no imprimir el stack a cada rato

    while (1) {
        rgb_color_t local_color;

        if (xSemaphoreTake(g_color_mutex, portMAX_DELAY) == pdTRUE) {
            local_color = g_current_color;
            xSemaphoreGive(g_color_mutex);
        } else {
            ESP_LOGW(TAG, "No se pudo tomar el mutex");
           continue;//vuelve al inicio del while(1)
        }

        rgb_led_set_color(local_color.r, local_color.g, local_color.b);

        vTaskDelay(pdMS_TO_TICKS(BLINK_ON_TIME_MS));//visto en clase

        rgb_led_off();

        vTaskDelay(pdMS_TO_TICKS(BLINK_OFF_TIME_MS));

        cycle_counter++;

        if (cycle_counter >= 20) {//si esto prende la led 500ms y apaga 500ms ,en 20 ciclos se muestra entonces cada 20 segundos MINIMO (si no hubieran interrupciones y otros eventos).
            cycle_counter = 0;

            UBaseType_t stack_free = uxTaskGetStackHighWaterMark(NULL);
            /*
            uxTaskGetStackHighWaterMark(NULL) devuelve el mínimo stack libre que tuvo TASK A desde que empezó a ejecutarse.
            Este valor se usa para ajustar usStackDepth (tercer parámetro de xTaskCreate()) si el valor observado es muy alto, 
            se puede reducir, si es muy bajo, hay que aumentarlo.
            * ojo ,no se ajusta automáticamente: se observa en el monitor y luego se cambia manualmente el valor de usStackDepth.*
            */

            ESP_LOGI(
                TAG,
                "Stack minimo libre TASK A: %u words",
                (unsigned int)stack_free
            );
        }
    }
}