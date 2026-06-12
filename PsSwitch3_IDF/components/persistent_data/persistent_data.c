#include "persistent_data.h"
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "Global.h"

#define NAMESPACE   "storage"
#define KEY_NAME    "device_name"
#define KEY_START   "start_state"

// ================================
// INIT
// ================================
void persistent_data_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

// ================================
// DEVICE NAME
// ================================

bool persistent_data_read_name(char *buffer)
{
    if (!buffer || device_name_max_len == 0) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        strncpy(buffer, "PS Switch", device_name_max_len - 1);
        buffer[device_name_max_len - 1] = '\0';
        return false;
    }

    size_t required_size = device_name_max_len;

    err = nvs_get_str(handle, KEY_NAME, buffer, &required_size);

    nvs_close(handle);

    if (err != ESP_OK) {
        strncpy(buffer, "PS Switch", device_name_max_len - 1);
        buffer[device_name_max_len - 1] = '\0';
        return false;
    }

    return true;
}

void persistent_data_write_name(const char *name)
{
    if (!name) return;

    nvs_handle_t handle;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return;

    nvs_set_str(handle, KEY_NAME, name);
    nvs_commit(handle);

    nvs_close(handle);
}

// ================================
// START STATE (key: "start_state")
// ================================

bool persistent_data_get_start_state(void)
{
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return false;

    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, KEY_START, &value);

    nvs_close(handle);

    if (err != ESP_OK)
        return false;

    return value != 0;
}

void persistent_data_set_start_state(bool state)
{
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return;

    nvs_set_u8(handle, KEY_START, state ? 1 : 0);
    nvs_commit(handle);

    nvs_close(handle);
}