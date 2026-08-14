idf_component_register(
    SRCS "hue.c"
    INCLUDE_DIRS "."
    REQUIRES
        config
    PRIV_REQUIRES
        persistent_data
        nvs_flash
        esp_driver_gpio
        esp_adc
)
