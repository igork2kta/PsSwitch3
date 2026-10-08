#include "wifi_manager.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

static const char *TAG = "WIFI_MANAGER";

static bool wifi_connected = false;
static bool ap_running = false;
static bool scan_in_progress = false;

bool get_scan_in_progress(void)
{
    return scan_in_progress;
}
void set_scan_in_progress(bool value)
{
    scan_in_progress = value;
}

static void start_ap_mode(void)
{
    if (ap_running)
        return;

    wifi_config_t ap = {
        .ap = {
            .ssid = "ESP32-SETUP",
            .password = "12345678",
            .ssid_len = 0,
            .channel = 11,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
            .beacon_interval = 100
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));

    ap_running = true;

    ESP_LOGI(TAG, "AP iniciado");
}

static void stop_ap_mode(void)
{
    if (!ap_running)
        return;

    ESP_LOGI(TAG, "Desabilitando AP");

    // Mantém o WiFi ativo apenas em STA.
    // O AP deixa de existir.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ap_running = false;
}

/* ===== Eventos ===== */

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:

                ESP_LOGI(TAG, "STA iniciado");
                esp_wifi_connect();

                break;

            case WIFI_EVENT_STA_DISCONNECTED:

                ESP_LOGW(TAG, "WiFi desconectado");

                wifi_connected = false;

                 //Reabre AP para permitir reconfiguração.

                if (!ap_running)
                {
                    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);

                    if (err == ESP_ERR_WIFI_STOP_STATE)
                    {
                        ESP_LOGI(TAG, "WiFi está sendo parado, ignorando evento.");
                        return;
                    }

                    ESP_ERROR_CHECK(err);

                    start_ap_mode();
                }

                ESP_LOGI(TAG, "Evento WiFi: %d", scan_in_progress);
                if(!scan_in_progress)
                    esp_wifi_connect();
            
                break;

            default:
                break;
        }
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG,
                 "Conectado! IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        wifi_connected = true;

        /*
         * Conectou com sucesso:
         * fecha o AP.
         */
        stop_ap_mode();

        ESP_LOGI(TAG, "Wi-Fi conectado!");
    }
}

/* ===== Init ===== */

esp_err_t wifi_manager_init(const char *device_name)
{
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    
    // Configura o hostname do dispositivo para o nome fornecido, é o que aparece no roteador
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, device_name));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );

    // Configuração STA salva automaticamente na NVS.
    ESP_ERROR_CHECK(
        esp_wifi_set_storage(WIFI_STORAGE_FLASH)
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            NULL
        )
    );

    ///Sempre inicia em AP+STA.
    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_APSTA)
    );

    start_ap_mode();

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    return ESP_OK;
}

/* ===== Configuração via Web ===== */

esp_err_t wifi_manager_save_and_connect(const char *ssid,
                                        const char *pass)
{
    //connect_in_progress = true;
    esp_wifi_disconnect();
    
    wifi_config_t sta = {0};

    strncpy(
        (char*)sta.sta.ssid,
        ssid,
        sizeof(sta.sta.ssid) - 1
    );

    strncpy(
        (char*)sta.sta.password,
        pass,
        sizeof(sta.sta.password) - 1
    );

    ESP_LOGI(TAG, "Salvando WiFi: %s", ssid);

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_STA, &sta)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_APSTA)
    );

    start_ap_mode();

    
    ESP_ERROR_CHECK(
        esp_wifi_connect()
    );
    
    
    //connect_in_progress = false;

    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return wifi_connected;
}

int8_t wifi_manager_get_rssi(void)
{
    wifi_ap_record_t ap_info;

    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
        return -127;

    return ap_info.rssi;
}

const char *wifi_manager_get_ssid(void)
{
    static char ssid[33];

    wifi_ap_record_t ap_info;

    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return "Não conectado";
    }

    memcpy(ssid, ap_info.ssid, sizeof(ap_info.ssid));
    ssid[sizeof(ssid) - 1] = '\0';

    return ssid;
}