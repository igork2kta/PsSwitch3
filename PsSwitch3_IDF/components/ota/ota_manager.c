#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "hue.h"

static const char *TAG = "OTA";

esp_err_t ota_put_handler(httpd_req_t *req)
{

    if (!get_ota_state()) {
        ESP_LOGE(TAG, "OTA não habilitada");
        httpd_resp_send_err(
            req,
            HTTPD_403_FORBIDDEN,
            "OTA não habilitada"
        );
    }


    esp_ota_handle_t ota_handle;

    const esp_partition_t *ota_partition =
        esp_ota_get_next_update_partition(NULL);

    if (ota_partition == NULL) {

        ESP_LOGE(TAG, "Sem particao OTA");

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "No OTA partition"
        );

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Iniciando OTA");

    esp_err_t err = esp_ota_begin(
        ota_partition,
        OTA_SIZE_UNKNOWN,
        &ota_handle
    );

    if (err != ESP_OK) {

        ESP_LOGE(TAG, "esp_ota_begin falhou");

        return ESP_FAIL;
    }

    char buf[1024];
    int received;

    while ((received = httpd_req_recv(
                req,
                buf,
                sizeof(buf))) > 0) {

        err = esp_ota_write(
            ota_handle,
            buf,
            received
        );

        if (err != ESP_OK) {

            ESP_LOGE(TAG, "esp_ota_write falhou");

            esp_ota_end(ota_handle);

            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Recebido: %d bytes", received);
    }

    if (received < 0) {

        ESP_LOGE(TAG, "Erro recebendo firmware");

        esp_ota_end(ota_handle);

        return ESP_FAIL;
    }

    err = esp_ota_end(ota_handle);

    if (err != ESP_OK) {

        ESP_LOGE(TAG, "esp_ota_end falhou");

        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(
        ota_partition
    );

    if (err != ESP_OK) {

        ESP_LOGE(TAG, "esp_ota_set_boot_partition falhou");

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA concluido");

    httpd_resp_sendstr(req, "OK");

    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();

    return ESP_OK;
}

void ota_register_uri(httpd_handle_t server)
{
    httpd_uri_t ota_uri = {
        .uri      = "/update",
        .method   = HTTP_PUT,
        .handler  = ota_put_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(
        server,
        &ota_uri
    );
}

void ota_confirm_running_firmware()
{
    const esp_partition_t *running =
        esp_ota_get_running_partition();

    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(
            running,
            &state) == ESP_OK)
    {
        if (state ==
            ESP_OTA_IMG_PENDING_VERIFY)
        {
            ESP_LOGI(
                TAG,
                "Confirmando firmware OTA"
            );

            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
}