idf_component_register(
    SRCS "task_c.c"
    INCLUDE_DIRS "."
    REQUIRES shared_types rgb_led freertos log esp_timer
)