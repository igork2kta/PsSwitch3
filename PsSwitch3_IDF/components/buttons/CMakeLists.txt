idf_component_register(
    SRCS   
        "buttons_handler.c"
             
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        ssdp
        hue
        webserver
        esp_driver_gpio
        esp_timer
        config
        driver
        esp_driver_touch_sens
)
