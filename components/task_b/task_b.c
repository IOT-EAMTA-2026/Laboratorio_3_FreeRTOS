#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "shared_types.h"
#include "task_b.h"

// --- CONFIGURACIONES ---
#define ECHO_UART_PORT_NUM      UART_NUM_0
#define ECHO_UART_BAUD_RATE     115200
#define BUF_SIZE                1024

static const char *TAG = "UART_terminal";

void task_b(void *arg)
{
    QueueHandle_t led_cmd_queue = (QueueHandle_t)arg;

    if (led_cmd_queue == NULL) {
        ESP_LOGE(TAG, "No se recibio la cola de comandos");
        vTaskDelete(NULL);
    }

    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(
        ECHO_UART_PORT_NUM,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    uint8_t line_buffer[BUF_SIZE];
    int line_index = 0;
    led_command_t led_cmd;

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
                    line_buffer[line_index] = '\0'; // Terminamos el string de forma segura
                    
                    ESP_LOGI(TAG, "Línea completa recibida: %s. Procesando...", (char *)line_buffer);

                    // --- PARSEO INMEDIATO DIRECTO EN LA TAREA ---
                    memset(&led_cmd, 0, sizeof(led_command_t)); // Limpiamos estructura

                    int espacio_idx = -1;
                    for (int i = 0; i < line_index; i++) {
                        if (line_buffer[i] == ' ') {
                            line_buffer[i] = '\0'; // Dividimos el string en dos partes
                            espacio_idx = i;
                            break;
                        }
                    }

                    char *color_str = (char *)line_buffer;
                    bool color_valido = true;

                    if (strcmp(color_str, "ROJO") == 0) {
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
                        int delay_s = atoi((char *)(line_buffer + espacio_idx + 1));
                        led_cmd.delay_s = delay_s;

                        ESP_LOGI(TAG, "Comando Listo -> R:%d G:%d B:%d | Delay: %d s", 
                                 led_cmd.color.r, led_cmd.color.g, led_cmd.color.b, led_cmd.delay_s);

                        // Enviamos la estructura directo a la cola global de hardware
                        xQueueSend(led_cmd_queue, &led_cmd, portMAX_DELAY);
                    }

                    // Reiniciamos el índice del búfer para la próxima línea
                    line_index = 0; 
                }
            } 
            else if (line_index < (BUF_SIZE - 1)) {
                line_buffer[line_index++] = byte_recibido;
            }
        }
    }
}