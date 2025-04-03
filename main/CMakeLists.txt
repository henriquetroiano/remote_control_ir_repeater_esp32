idf_component_register(
    SRCS
        "main.c"
        "led.c"
        "screen.c"
        "wifi.c"
        "write.c"
        "keyboard.c"
        "commander.c"
        "infrared.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_wifi
        nvs_flash
        driver
        esp_timer
)
