idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES rgb_led task_b delay task_c
    INCLUDE_DIRS "." "../components"
    REQUIRES freertos driver 
    EMBED_FILES
)