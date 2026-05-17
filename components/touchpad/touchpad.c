#include "touchpad.h"

#include <stdio.h>
#include "esp_err.h"
#include "touch_element/touch_element.h"
#include "touch_element/touch_button.h"

static const touch_pad_t touch_channels[TOUCHPAD_BUTTON_COUNT] = {
    TOUCH_PAD_NUM1,   // IO1
    TOUCH_PAD_NUM2,   // IO2
    TOUCH_PAD_NUM3,   // IO3
    TOUCH_PAD_NUM5,   // IO5
    TOUCH_PAD_NUM6,   // IO6
    TOUCH_PAD_NUM11   // IO11
};

static touch_button_handle_t button_handle[TOUCHPAD_BUTTON_COUNT];

static bool button_state[TOUCHPAD_BUTTON_COUNT] = {false};
static bool initialized = false;

static void touch_button_callback(touch_button_handle_t handle, //estos parámetros los pone la librería.
                                  touch_button_message_t *message,//cuando pasa un evento la libreria llama a callback de una y el pasa esos argumentos
                                  void *arg)
{
    uint8_t button_index = (uint8_t)(uintptr_t)arg;

    if (message->event == TOUCH_BUTTON_EVT_ON_PRESS) {
        button_state[button_index] = true;
    } else if (message->event == TOUCH_BUTTON_EVT_ON_RELEASE) {
        button_state[button_index] = false;
    }
}

void touchpad_init(void)
{
    if (initialized) {
        return;
    }

    // Config general de la libreria touch de espressif
    touch_elem_global_config_t global_cfg = TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_element_install(&global_cfg));

    // Config general para botones capacitivos
    touch_button_global_config_t button_global_cfg = TOUCH_BUTTON_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_button_install(&button_global_cfg));

    for (uint8_t i = 0; i < TOUCHPAD_BUTTON_COUNT; i++) {
        touch_button_config_t button_cfg = {
            .channel_num = touch_channels[i],
            .channel_sens = 0.10f,   // sensibilidad del boton, si no detecta podria probar 0.05f
        };

        ESP_ERROR_CHECK(touch_button_create(&button_cfg, &button_handle[i]));

        ESP_ERROR_CHECK(touch_button_subscribe_event(
            button_handle[i],
            TOUCH_ELEM_EVENT_ON_PRESS | TOUCH_ELEM_EVENT_ON_RELEASE,
            (void *)(uintptr_t)i //aca establece que i es el arg que recibe la funcion touch_button_callback
                    //// si no ponía (uintptr_t) saltaba un warning porque estaba casteando un entero directo a puntero,
                     // y un int ( uint8_t = 1 byte) no tiene el mismo tamaño que un puntero,
                     // entonces puede quedar mal la conversión.
                     // uintptr_t es un entero del mismo tamaño que un puntero, hace de "intermediario"
        ));

        ESP_ERROR_CHECK(touch_button_set_dispatch_method(
            button_handle[i],
            TOUCH_ELEM_DISP_CALLBACK
        ));

        ESP_ERROR_CHECK(touch_button_set_callback(
            button_handle[i],
            touch_button_callback
        ));

        printf("Boton %d configurado en canal %d\n", i, touch_channels[i]);
    }

    ESP_ERROR_CHECK(touch_element_start());

    initialized = true;
    printf("Touchpad pronto usando touch_element de Espressif\n");
}

bool touchpad_is_pressed(uint8_t button_index)
{
    if (button_index >= TOUCHPAD_BUTTON_COUNT) {
        return false;
    }

    return button_state[button_index];
}