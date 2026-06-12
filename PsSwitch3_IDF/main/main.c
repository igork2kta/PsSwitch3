#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "webserver.h"
#include "hue.h"
#include "buttons_handler.h"
#include "ota_manager.h"

static void on_wifi_connected(void)
{
    ESP_LOGI("MAIN", "Wi-Fi conectado!");
    // aqui você pode parar o portal ou subir outra API
}

void app_main(void)
{
    button_init();
    esp_netif_init();
    esp_event_loop_create_default();
    
    //hue
    init();

    wifi_manager_init(on_wifi_connected);
    start_webserver();

    ESP_LOGI("MAIN", "Aplicação iniciada!");

    
    ota_confirm_running_firmware();
}
