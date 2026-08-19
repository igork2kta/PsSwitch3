#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "webserver.h"
#include "hue.h"
#include "buttons_handler.h"
#include "ota_manager.h"


void app_main(void)
{
    button_init();
    esp_netif_init();
    esp_event_loop_create_default();
    
    //hue
    init();

    wifi_manager_init(get_device_name());
    start_webserver();

    ESP_LOGI("MAIN", "Aplicação iniciada!");

    
    ota_confirm_running_firmware();
}
