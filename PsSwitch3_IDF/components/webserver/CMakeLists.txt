idf_component_register(
    SRCS    "webserver.c"
             
    INCLUDE_DIRS "."
    EMBED_FILES "provisioning_page.html.gz"
                "web_interface.html.gz"
    PRIV_REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_server
        wifi_manager
        ssdp
        hue
        config
        esp_driver_gpio
        esp_timer
        ota
        
)
