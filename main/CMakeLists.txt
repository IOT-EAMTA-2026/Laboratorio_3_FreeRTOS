idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES esp_wifi nvs_flash esp_http_server cjson rgb_led touch_element touchpad  delay
    INCLUDE_DIRS "."
    EMBED_FILES
        "web/index.html"
        "web/style.css"
        "web/app.js"
)