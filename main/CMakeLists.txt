idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES cjson rgb_led task_b delay 
    INCLUDE_DIRS "."
    REQUIRES freertos driver esp_log
    EMBED_FILES
)