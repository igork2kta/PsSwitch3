#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t ota_put_handler(httpd_req_t *req);
void ota_register_uri(httpd_handle_t server);
void ota_confirm_running_firmware(void);