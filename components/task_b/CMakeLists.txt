idf_component_register(
    SRCS "task_b.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart esp_driver_gpio shared_types freertos
)