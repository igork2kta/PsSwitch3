#pragma once

#include "esp_err.h"
#include <stdbool.h>

bool get_scan_in_progress(void);
bool set_scan_in_progress(bool value);

typedef void (*wifi_connected_cb_t)(void);

esp_err_t wifi_manager_init(wifi_connected_cb_t cb);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_save_and_connect(const char *ssid, const char *pass);
