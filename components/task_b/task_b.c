#ifndef TASK_B
#define TASK_B

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "task_b.h" 

/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)
#define BUF_SIZE (1024)

static const char *TAG = "UART_terminal";

// Handlers de FreeRTOS
static QueueHandle_t uart_rx_queue = NULL; 
static QueueHandle_t led_cmd_queue = NULL;

static void parse_task(void *arg);

static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // CREACIÓN DE COLAS
    // Cola para pasar el string de echo_task a parse_task (guardamos arrays de texto)
    uart_rx_queue = xQueueCreate(5, BUF_SIZE); 
    // Cola hipotética para enviar el comando procesado a tus LEDs
    led_cmd_queue = xQueueCreate(5, sizeof(led_command_t)); 

    // 2. Búfer estático para ir armando la línea de texto
    uint8_t line_buffer[BUF_SIZE];
    int line_index = 0;

    ESP_LOGI(TAG, "Terminal lista. Escribe un COLOR<espacio>SEGUNDOS y presiona ENTER...");

    while (1) {
        uint8_t byte_recibido;
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, &byte_recibido, 1, pdMS_TO_TICKS(10));

        if (len > 0) {
            // Eco en espejo para la consola
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) &byte_recibido, 1);

            // Al presionar ENTER (detectamos salto de línea)
            if (byte_recibido == '\n' || byte_recibido == '\r') {
                if (line_index > 0) { 
                    line_buffer[line_index] = '\0'; // Terminamos el string de forma segura

                    // Enviamos TODO el texto a la cola de parseo (No bloquea si está llena)
                    if (xQueueSend(uart_rx_queue, &line_buffer, pdMS_TO_TICKS(10)) != pdPASS) {
                        ESP_LOGW(TAG, "Cola de parseo llena. Ignorando comando.");
                    }

                    line_index = 0; // Reiniciamos índice para la siguiente línea
                }
            } 
            else if (line_index < (BUF_SIZE - 1)) {
                line_buffer[line_index++] = byte_recibido;
            }
        }
    }
}


// TAREA 2: PROCESA EL TEXTO CUANDO LLEGA A LA COLA
static void parse_task(void *arg)
{
    uint8_t texto_recibido[BUF_SIZE];
    led_command_t led_cmd;

    while(1) {
        // Esta tarea se queda "dormida" (bloqueada) sin consumir CPU 
        // hasta que xQueueReceive reciba una línea completa desde echo_task
        if (xQueueReceive(uart_rx_queue, &texto_recibido, portMAX_DELAY) == pdTRUE) {
            
            ESP_LOGI(TAG, "Parseando texto: %s", (char *)texto_recibido);

            // Inicializamos la estructura limpia
            memset(&led_cmd, 0, sizeof(led_command_t));

            // Buscamos el espacio para dividir las cadenas
            int len_texto = strlen((char *)texto_recibido);
            int espacio_idx = -1;
            for (int i = 0; i < len_texto; i++) {
                if (texto_recibido[i] == ' ') {
                    texto_recibido[i] = '\0'; // Dividimos el string
                    espacio_idx = i;
                    break;
                }
            }

            char *color_str = (char *)texto_recibido;
            bool color_valido = true;

            if (strcmp(color_str, "ROJO") == 0) {   
                led_cmd.color.r = 255;
            } else if (strcmp(color_str, "VERDE") == 0) {
                led_cmd.color.g = 255;
            } else if (strcmp(color_str, "AZUL") == 0) {
                led_cmd.color.b = 255;
            } else {
                ESP_LOGW(TAG, "Color desconocido: %s", color_str);
                color_valido = false;
            }

            if (color_valido && espacio_idx != -1) {
                // El número arranca justo después del espacio que convertimos en \0
                int delay_s = atoi((char *)(texto_recibido + espacio_idx + 1));
                led_cmd.delay_s = delay_s;

                ESP_LOGI(TAG, "Comando Generado -> R:%d G:%d B:%d | Delay: %d s", 
                         led_cmd.color.r, led_cmd.color.g, led_cmd.color.b, led_cmd.delay_s);

                // Aquí enviarías 'led_cmd' a la tarea final que maneja los pines físicos de los LEDs
                // xQueueSend(led_cmd_queue, &led_cmd, portMAX_DELAY);
            }
        }
    }
}