idf_component_register(
    SRCS "task_c.c"
    INCLUDE_DIRS "." ".." "../shared_types/include"
    REQUIRES freertos log
)