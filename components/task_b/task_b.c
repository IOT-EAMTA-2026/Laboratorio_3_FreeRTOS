#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "shared_types.h"
#include "task_b.h"

// --- CONFIGURACIONES UART ---
#define ECHO_UART_PORT_NUM      (0)         // Usamos el UART0
#define ECHO_UART_BAUD_RATE     (115200)    // Velocidad estándar
#define BUF_SIZE                (128)      // Tamaño del buffer de lectura
//#define ECHO_TASK_STACK_SIZE    (3072)    // Memoria para el stack de la tarea definida en app_main

// --- CONFIGURACIÓN DE PINES ---
#define ECHO_TEST_TXD           (43)       // Cambiar el pin TX (UART0)
#define ECHO_TEST_RXD           (44)       // Cambiar el pin RX (UART0)


#define QUEUE_SEND_TIMEOUT_MS   100

static const char *TAG = "UART_terminal";

static void send_status(void)
{
    rgb_color_t local_color = {0};

    if (xSemaphoreTake(g_color_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        local_color = g_current_color;
        xSemaphoreGive(g_color_mutex);

        char status_msg[64];

        snprintf(    //Guardar de forma segura en un espacio de memoria (un buffer)
            status_msg,
            sizeof(status_msg),
            "STATUS: R=%u G=%u B=%u | timers pendientes=%lu\r\n",
            local_color.r,
            local_color.g,
            local_color.b,
            (unsigned long)g_pending_timers
        );

        uart_write_bytes(UART_NUM_0, status_msg, strlen(status_msg)); // Enviar el mensaje de estado por UART
        //ESP_LOGI(TAG, "%s", status_msg); Prueba de imprimir el status también en el log
    } else {
        ESP_LOGW(TAG, "No se pudo tomar el mutex para STATUS");
    }
}



void task_b(void *arg){

    QueueHandle_t led_cmd_queue = (QueueHandle_t)arg; // Recibimos la cola de comandos como argumento

    if (led_cmd_queue == NULL) {
        ESP_LOGE(TAG, "No se recibio la cola de comandos");
        vTaskDelete(NULL);
    }

    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE, //velocidad de la comunicación
        .data_bits = UART_DATA_8_BITS,    //tamaño del paquete de datos.
        .parity    = UART_PARITY_DISABLE, //El bit de paridad (Chequeo de errores).
        .stop_bits = UART_STOP_BITS_1,    //Número de bits de parada.
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, //Control de flujo por hardware (RTS/CTS).
        .source_clk = UART_SCLK_DEFAULT,      //Reloj fuente para el UART.
    };

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(
        ECHO_UART_PORT_NUM,   // Número del puerto UART a configurar (44 RX y 43 TX en ESP32)
        ECHO_TEST_TXD,  // Cambiar el pin TX  (usamos el pin 43)
        ECHO_TEST_RXD,  // Cambiar el pin RX  (usamos el pin 44)
        UART_PIN_NO_CHANGE, // No cambiar el pin RTS (Request to Send)
        UART_PIN_NO_CHANGE  // No cambiar el pin CTS (Clear to Send)
    ));

    uint8_t line_buffer[BUF_SIZE]; // Buffer para almacenar la línea de comando recibida
    int line_index = 0;            // Índice para rastrear la posición en el buffer de la línea
    led_command_t led_cmd;         // Variable para almacenar el comando de LED parseado
    uint32_t cycle_counter = 0;    // Contador para monitorear el uso de la pila

    ESP_LOGI(TAG, "Terminal lista. Escribe un COLOR<espacio>SEGUNDOS y presiona ENTER...");
    while (1) {
        uint8_t byte_recibido;
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, &byte_recibido, 1, pdMS_TO_TICKS(10));

        if (len > 0) {
            // Eco en espejo en la consola
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) &byte_recibido, 1);

            // Al presionar ENTER (detectamos fin de línea)
            if (byte_recibido == '\n' || byte_recibido == '\r') {
                if (line_index > 0) { 
                    line_buffer[line_index] = '\0'; // Termina el string de forma segura
                    
                    ESP_LOGI(TAG, "Línea completa recibida: %s. Procesando...", (char *)line_buffer);

                    cycle_counter++;

                    if (cycle_counter >= 5) {
                        cycle_counter = 0;

                        ESP_LOGI(
                            TAG,
                            "Stack minimo libre TASK B: %u words",
                            (unsigned int)uxTaskGetStackHighWaterMark(NULL) //El mínimo de memoria libre que tuvo la tarea desde que empezó a ejecutarse
                        );
                    }

                        // --- PARSEO INMEDIATO DIRECTO EN LA TAREA ---
                    memset(&led_cmd, 0, sizeof(led_command_t));  // Reiniciar la estructura del comando antes de llenarla
                     /*
                     * STATUS no se encola porque no implica cambiar el color
                     * luego de un delay. Solo consulta el estado actual.
                     */
                    if (strcmp((char *)line_buffer, "STATUS") == 0) {
                        send_status();
                        line_index = 0;
                        continue;
                    }

                    int espacio_idx = -1;
                    for (int i = 0; i < line_index; i++) {
                        if (line_buffer[i] == ' ') {
                            line_buffer[i] = '\0'; // Divide el string en dos partes
                            espacio_idx = i;
                            break;
                        }
                    }

                    char *color_str = (char *)line_buffer;
                    bool color_valido = true;

                    if (strcmp(color_str, "ROJO") == 0) {   //strcmp es para comparar strings, devuelve 0 si son iguales
                        led_cmd.color.r = 255;
                        led_cmd.color.g = 0;
                        led_cmd.color.b = 0;

                    } else if (strcmp(color_str, "VERDE") == 0) {
                        led_cmd.color.r = 0;
                        led_cmd.color.g = 255;
                        led_cmd.color.b = 0;

                    } else if (strcmp(color_str, "AZUL") == 0) {
                        led_cmd.color.r = 0;
                        led_cmd.color.g = 0;
                        led_cmd.color.b = 255;

                    } else if (strcmp(color_str, "BLANCO") == 0) {
                        led_cmd.color.r = 255;
                        led_cmd.color.g = 255;
                        led_cmd.color.b = 255;

                    } else if (strcmp(color_str, "AMARILLO") == 0) {
                        led_cmd.color.r = 255;
                        led_cmd.color.g = 255;
                        led_cmd.color.b = 0;

                    } else if (strcmp(color_str, "CYAN") == 0) {
                        led_cmd.color.r = 0;
                        led_cmd.color.g = 255;
                        led_cmd.color.b = 255;

                    } else if (strcmp(color_str, "MAGENTA") == 0) {
                        led_cmd.color.r = 255;
                        led_cmd.color.g = 0;
                        led_cmd.color.b = 255;

                    } else if (strcmp(color_str, "NARANJA") == 0) {
                        led_cmd.color.r = 255;
                        led_cmd.color.g = 80;
                        led_cmd.color.b = 0;

                    } else if (strcmp(color_str, "VIOLETA") == 0) {
                        led_cmd.color.r = 128;
                        led_cmd.color.g = 0;
                        led_cmd.color.b = 255;

                    } else if (strcmp(color_str, "APAGADO") == 0) {
                        led_cmd.color.r = 0;
                        led_cmd.color.g = 0;
                        led_cmd.color.b = 0;

                    } else {
                        ESP_LOGW(TAG, "Color desconocido: %s", color_str);
                        color_valido = false;
                    }

                    if (color_valido && espacio_idx != -1) {
                        // El número está justo tras el espacio convertido en '\0'
                        int delay_s = atoi((char *)(line_buffer + espacio_idx + 1)); //Significa ASCII to Integer
                        led_cmd.delay_s = delay_s;

                        ESP_LOGI(TAG, "Comando Listo -> R:%d G:%d B:%d | Delay: %d s", 
                                 led_cmd.color.r, led_cmd.color.g, led_cmd.color.b, led_cmd.delay_s);

                        if (xQueueSend( led_queue, &led_cmd, pdMS_TO_TICKS(100) == pdTRUE)) { // Enviar el comando a la cola con un timeout de 100 ms
                            ESP_LOGI(TAG, "Comando enviado a la cola correctamente");
                        } else {
                            ESP_LOGW(TAG, "No se pudo enviar el comando: cola llena");
                        }
                    }
                    // Reinicia el índice del búfer para la próxima línea
                    line_index = 0; 
                }
            } 
            else if (line_index < (BUF_SIZE - 1)) {      //"Cinta transportadora" que va recolectando tu comando letra por letr
                line_buffer[line_index++] = byte_recibido;   
            }
        }
    }
    }