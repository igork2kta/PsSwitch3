#pragma once

#include "esp_err.h"
#include <stdbool.h>

bool get_scan_in_progress(void);
void set_scan_in_progress(bool value);

typedef void (*wifi_connected_cb_t)(void);

esp_err_t wifi_manager_init(const char *device_name);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_save_and_connect(const char *ssid, const char *pass);
int8_t wifi_manager_get_rssi(void);
const char *wifi_manager_get_ssid(void);
