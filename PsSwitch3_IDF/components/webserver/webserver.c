#include "webserver.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "ssdp.h"
#include "templates.h"
#include "hue.h"
#include "Global.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "wifi_manager.h"
#include "ota_manager.h"

#define MAX_APS 20

static const char *TAG = "WEBSERVER";

/* ===== Credenciais ===== */
static char wifi_ssid[32];
static char wifi_pass[64];

static char hue_username[32];

/* ===== HTML ===== */
extern const uint8_t provisioning_page_html_gz_start[] asm("_binary_provisioning_page_html_gz_start");
extern const uint8_t provisioning_page_html_gz_end[]   asm("_binary_provisioning_page_html_gz_end");

extern const uint8_t web_interface_html_gz_start[] asm("_binary_web_interface_html_gz_start");
extern const uint8_t web_interface_html_gz_end[]   asm("_binary_web_interface_html_gz_end");

/* ===== Utils ===== */

static void url_decode(char *src)
{
    char *dst = src;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = { src[1], src[2], '\0' };
            *dst = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
    }
    *dst = '\0';
}


static bool get_body(httpd_req_t *req,
                     char *buffer,
                     size_t buffer_size)
{
    if (req->content_len <= 0) {
        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "Body vazio");
        return false;
    }

    if (req->content_len >= buffer_size) {
        httpd_resp_send_err(req,
                            HTTPD_413_CONTENT_TOO_LARGE,
                            "Body muito grande");
        return false;
    }

    int total_len = 0;

    while (total_len < req->content_len) {

        int recv_len = httpd_req_recv(
            req,
            buffer + total_len,
            req->content_len - total_len
        );

        if (recv_len <= 0) {
            httpd_resp_send_err(req,
                                HTTPD_400_BAD_REQUEST,
                                "Erro ao receber body");
            return false;
        }

        total_len += recv_len;
    }

    buffer[total_len] = '\0';

    return true;
}

int get_json_int(const char* data, const char* key) 
{ 
    char pattern[16]; 
    snprintf(pattern, sizeof(pattern), "\"%s\":", key); 
    const char* pos = strstr(data, pattern); 
    if (!pos) return 0; 
    pos += strlen(pattern); 
    
    // atoi lê o que tem a partir da posição indicada até o primeiro caractere inválido e converte pra int, ele ignora espaços e tabulações mas se vier algum caractere invalido depois da posição retorna 0 
    return atoi(pos); 
}


/* ===== HTTP Handlers ===== */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    //esp_wifi_disconnect();
    set_scan_in_progress(true);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    size_t size = provisioning_page_html_gz_end -
                  provisioning_page_html_gz_start;

    return httpd_resp_send(req,
        (const char *)provisioning_page_html_gz_start,
        size);
    }

static esp_err_t interface_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    size_t size = web_interface_html_gz_end -
                  web_interface_html_gz_start;

    return httpd_resp_send(req,
        (const char *)web_interface_html_gz_start,
        size);

}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    //esp_wifi_disconnect();
    set_scan_in_progress(true);

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "WiFi scan failed");

    }

    set_scan_in_progress(false);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > MAX_APS) ap_count = MAX_APS;

    wifi_ap_record_t ap_records[MAX_APS];
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");

    for (int i = 0; i < ap_count; i++) {
        char line[128];
        snprintf(line, sizeof(line),
            "{\"ssid\":\"%s\",\"rssi\":%d}%s",
            (char *)ap_records[i].ssid,
            ap_records[i].rssi,
            (i == ap_count - 1) ? "" : ",");
        httpd_resp_sendstr_chunk(req, line);
    }

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);


    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    httpd_query_key_value(buf, "ssid", wifi_ssid, sizeof(wifi_ssid));
    httpd_query_key_value(buf, "pass", wifi_pass, sizeof(wifi_pass));

    url_decode(wifi_ssid);
    url_decode(wifi_pass);

    ESP_LOGI(TAG, "Recebido WiFi SSID: %s", wifi_ssid);

    esp_err_t err = wifi_manager_save_and_connect(wifi_ssid, wifi_pass);

    if (err != ESP_OK) {

        ESP_LOGE(TAG,
                 "Erro ao salvar WiFi");

        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");

    }

    return httpd_resp_sendstr(req, "Configurações recebidas. Tentando conectar...");

}
/*
static esp_err_t stop_get_handler(httpd_req_t *req)
{
    ssdp_stop();
    return httpd_resp_sendstr(req, "Desligando descoberta!");

}

static esp_err_t start_get_handler(httpd_req_t *req)
{
    ssdp_start();
    return httpd_resp_sendstr(req, "Ligando descoberta!");
}
*/
static esp_err_t description_xml_handler(httpd_req_t *req)
{
    char ip_str[16] = "0.0.0.0";
    uint8_t mac[6] = {0};
    char mac_str[18];

    // IP
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        esp_ip4addr_ntoa(&ip.ip, ip_str, sizeof(ip_str));
    }

    // MAC
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char xml[1024];

    int len = template_build_description_xml(
        xml,
        sizeof(xml),
        ip_str,
        mac_str
    );
 
    httpd_resp_set_type(req, "text/xml");
    return httpd_resp_send(req, xml, len);

}

static esp_err_t api_username_post_handler(httpd_req_t *req)
{
    /*
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
    }

    buf[len] = '\0';

    // Alexa sempre manda JSON com "devicetype"
    
    if (!strstr(buf, "\"devicetype\"")) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing devicetype");
    }
    */
  

    char response[128];
    int len = snprintf(response, sizeof(response),
        "[{\"success\":{\"username\":\"%s\"}}]",
        hue_username
    );

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, len);


}



static esp_err_t lights_get_handler(httpd_req_t *req)
{
    const char *uri = req->uri;

    const char *lights = strstr(uri, "/lights");

    uint8_t mac[6];
    char hue_uniqueid[40];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    snprintf(hue_uniqueid, sizeof(hue_uniqueid),
        "%02X:%02X:%02X:%02X:%02X:%02X:00:00-00",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );

    if (strcmp(lights, "/lights") == 0) {
        char response[150];

        int len = template_build_lights_short(
            response,
            sizeof(response),
            get_device_name(),
            PSSWITCH_TYPE,
            hue_uniqueid
        );

        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, response, len);
  
    }
    
    // Verifica se é "/lights/{id}"
    else if (strncmp(lights, "/lights/", 8) == 0) {

        const char *id = lights + 8; // aponta para o "1"
        ESP_LOGI(TAG, "Requisição para light ID: %s", id);

        char response[850];

        int len =   template_build_light_long(
            response,
            sizeof(response),
            get_device_name(),
            PSSWITCH_TYPE,
            hue_uniqueid,
            get_bri(),
            get_state(),
            get_timer_state(),
            get_timer_minutes(),
            get_start_state(),
            get_ota_state(),
            get_temperature(),
            get_thermal_shutdown_count(),
            SW_VERSION
        );

        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, response, len);
    }

    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
}


static esp_err_t state_put_handler(httpd_req_t *req)
{
    char body[35];
    if (!get_body(req, body, sizeof(body))) 
        return ESP_FAIL;

    bool state = (strstr(body, "true") != NULL); 
    uint8_t bri = get_json_int(body, "bri");
 
    set_state(state, bri);

    char response[96];
    int len = template_build_light_state_success(response,
                                       sizeof(response),
                                       get_state(),
                                       get_bri());

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, len);

}

static esp_err_t timer_put_handler(httpd_req_t *req)
{
    char body[45];
    if (!get_body(req, body, sizeof(body))) 
        return ESP_FAIL;

    bool timer_state = (strstr(body, "true") != NULL); 
    uint8_t bri = get_json_int(body, "bri");
    int minutes = (uint16_t)get_json_int(body, "minutes");

    if(minutes == 0) {

        char response[41];
        template_build_message(
                response,sizeof(response),
                "Missing minutes parameter"
            );
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, response);

    }

    set_timer(timer_state, bri, minutes);

    #if DEBUG_WEBSERVER
        ESP_LOGI(TAG, "TIMER ON=%d | BRI=%d | MINUTES=%d", timer_state, bri, minutes);
    #endif

    char response[128];
    int len = template_build_light_timer_success(response,
                                       sizeof(response),
                                       get_timer_state(),
                                       get_timer_minutes());

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, len);

}

static esp_err_t start_state_ota_put_handler(httpd_req_t *req)
{
    
    char body[30];
    if (!get_body(req, body, sizeof(body))) 
        return ESP_FAIL;

    bool state = (strstr(body, "true") != NULL); 


    if (strstr(req->uri, "/startState")) {
        set_start_state(state);
        ESP_LOGI(TAG, "START_STATE=%d", state);
    }

    else if (strstr(req->uri, "/otaState")) {
        set_ota_state(state);
        ESP_LOGI(TAG, "OTA=%d", state);
    }

    char resp[25];
    int len = template_build_message(
            resp,sizeof(resp),
            "Sucesso!"
        );

    

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, len);
}

static esp_err_t rename_put_handler(httpd_req_t *req)
{
    char body[45];
    if (!get_body(req, body, sizeof(body))) 
        return ESP_FAIL;

    const char* start = strstr(body, "\"name\":\"");
    if (!start) {
        char response[42];
        template_build_message(
                response,sizeof(response),
                "Campo nome não encontrado!"
            );

        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, response);
    }

    start += 8;

    const char* end = strchr(start, '"');
    if (!end) {
        char response[35];
        template_build_message(
                response,sizeof(response),
                "Json mal formatado!"
            );
            
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, response);
    }

    size_t len = end - start;

    // buffer temporário (stack, rápido e seguro)
    char newName[device_name_max_len]; // mesmo tamanho do device_name
    if (len >= device_name_max_len) {
        
        char response[32];
        template_build_message(
            response,sizeof(response),
            "Nome muito longo"
        );

        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, response);

    }

    memcpy(newName, start, len);
    newName[len] = '\0';

    set_device_name(newName);

    char resp[25];
    int resp_len = template_build_message(
            resp,sizeof(resp),
            "Sucesso!"
        );

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, resp_len);

}


void reset(void)
{
    ESP_LOGI("RESET", "Limpando WiFi e reiniciando...");

    // Para o Wi-Fi
    esp_wifi_stop();

    ESP_ERROR_CHECK(nvs_flash_deinit());

    esp_err_t err = nvs_flash_erase();
    ESP_LOGI(TAG, "nvs_flash_erase = %s", esp_err_to_name(err));

    ESP_ERROR_CHECK(err);

    err = nvs_flash_init();
    ESP_LOGI(TAG, "nvs_flash_init = %s", esp_err_to_name(err));

    ESP_ERROR_CHECK(err);
    
    esp_restart();
}

static esp_err_t reset_put_handler(httpd_req_t *req)
{
    char body[30];
    if (!get_body(req, body, sizeof(body))) 
        return ESP_FAIL;

    //Default body {"key":"f635g2vg3gn54H$"}                    
    if (strstr(body, "f635g2vg3gn54H$") != NULL){

        httpd_resp_set_type(req, "application/json");
  
        char response[25];
        int len = template_build_message(
                response,sizeof(response),
                "Sucesso!"
            );

        httpd_resp_send(req, response, len);
        
        vTaskDelay(pdMS_TO_TICKS(3000));

        reset();

        return ESP_OK;

        }
    else {
        char response[32];
        template_build_message(
                response,sizeof(response),
                "Unauthorized"
            );
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, response);
    }
        
}


static esp_err_t lights_handler(httpd_req_t *req)
{
    const char *uri = req->uri;

    //Se não tiver /lights ja pula fora
    const char *lights = strstr(uri, "/lights");
    if (!lights) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad URI");
    }
    
    if (req->method == HTTP_GET) {
        return lights_get_handler(req);
    }

    if (req->method == HTTP_PUT) {
        
        if (strstr(req->uri, "/state") ){
            return state_put_handler(req);
        }
        else if (strstr(req->uri, "/timer")) {
            return timer_put_handler(req);
        }
        else if (strstr(req->uri, "/startState") || strstr(req->uri, "/otaState")) {
            return start_state_ota_put_handler(req);
        }
        else if (strstr(req->uri, "/rename")) {
            return rename_put_handler(req);
        }
        else if (strstr(req->uri, "/reset")) {
            return reset_put_handler(req);
        }

        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Invalid PUT");
    }
    
    return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
}  


/* ===== Webserver ===== */

void start_webserver(void)
{
    uint8_t mac[6] = {0};

    // MAC da interface Wi-Fi STA
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    snprintf(hue_username, sizeof(hue_username),
        "esp32%02x%02x%02x%02x%02x%02x",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );

    ESP_LOGI(TAG, "Hue username gerado: %s", hue_username);

    //aparentemente existe um limite de uris que é 8, se precisa, aumentar
    //cfg.max_uri_handlers = 16;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.stack_size = 8192;
    // Número máximo de sockets (ajuste conforme a RAM disponível)
    cfg.max_open_sockets = 5; 

    // Timeout de 3 segundos de inatividade para fechar o socket automaticamente
    cfg.lru_purge_enable = true; // Expulsa a conexão mais antiga se faltar espaço
    cfg.recv_wait_timeout = 3;
    cfg.send_wait_timeout = 3;

    httpd_handle_t server;
    httpd_start(&server, &cfg);

    httpd_uri_t root = {
        .uri = "/", .method = HTTP_GET, .handler = root_get_handler
    };
    httpd_uri_t interface = {
        .uri = "/interface", .method = HTTP_GET, .handler = interface_get_handler
    };
    httpd_uri_t scan = {
        .uri = "/scan", .method = HTTP_GET, .handler = scan_get_handler
    };
    httpd_uri_t save = {
        .uri = "/save", .method = HTTP_POST, .handler = save_post_handler
    };
    httpd_uri_t description_xml = {
        .uri = "/description.xml", .method = HTTP_GET, .handler = description_xml_handler, .user_ctx = NULL
    };
    httpd_uri_t api = {
        .uri = "/api", .method = HTTP_POST, .handler = api_username_post_handler, .user_ctx = NULL
    };
    httpd_uri_t lights = {
        .uri = "/api/*", .method = HTTP_ANY, .handler = lights_handler, .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &interface);
    httpd_register_uri_handler(server, &scan);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &description_xml);
    httpd_register_uri_handler(server, &api);
    httpd_register_uri_handler(server, &lights);
    ota_register_uri(server);

}
