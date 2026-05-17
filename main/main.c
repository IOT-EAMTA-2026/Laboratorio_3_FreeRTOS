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