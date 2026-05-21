idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES rgb_led task_b delay task_c
    INCLUDE_DIRS "." "../components" "../components/shared_types/include" 
    "../components/task_b" "../components/task_c" "../components/rgb_led" "../components/delay"
    "../components/task_a" "../components/task_a/include"
    REQUIRES freertos driver 
    EMBED_FILES
)