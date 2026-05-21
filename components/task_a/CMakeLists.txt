idf_component_register(
    SRCS "task_a.c"
    INCLUDE_DIRS "include" "../shared_types/include"
    REQUIRES shared rgb_led freertos
)