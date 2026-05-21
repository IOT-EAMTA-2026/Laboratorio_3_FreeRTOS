idf_component_register(SRCS "task_b.c"
                    REQUIRES esp_driver_uart esp_driver_gpio
                    INCLUDE_DIRS "." "..")