<<<<<<< HEAD
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "rgb_led.h"
#include "touchpad.h"
#include "delay.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "freertos/semphr.h"

 #include "task_a.h" 
 #include "task_b.h" 
 #include "task_c.h" 

     // 2. Creas la tarea que ya programaste en la otra carpeta
    xTaskCreatePinnedToCore(
        tarea_motores,       // Función que está en mi_controlador.c
        "TareaMotores",      // Nombre de depuración
        3072,                // Tamaño de pila (Stack)
        NULL,                // Parámetros
        5,                   // Prioridad
        NULL,                // Manejador (Handle)
        0                    // Núcleo (Core 0)
    );
    
    // app_main puede terminar aquí, la tarea_motores seguirá corriendo sola
}
//xTaskCreate para cada tarea, con su respectiva función, nombre, stack size, parámetros, prioridad y handle (si es necesario)
=======
﻿#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>

#define BLINK_GPIO GPIO_NUM_2

void blink_task(void *pvParameter) {
    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    xTaskCreate(blink_task, "blink_task", 1024, NULL, 5, NULL);
}
>>>>>>> bf949327e613b6b869fe1d1ee1d7cd7ceff4f312
