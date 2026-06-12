idf_component_register(
    SRCS "persistent_data.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        config
)
