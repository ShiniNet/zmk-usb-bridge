#include "zmk_usb_bridge/persist.h"

#include <esp_log.h>
#include <string.h>

static const char *TAG = "zub_persist";

static zmk_usb_bridge_metadata_t g_metadata;
static bool g_has_metadata;

esp_err_t zmk_usb_bridge_persist_init(void) {
    memset(&g_metadata, 0, sizeof(g_metadata));
    g_has_metadata = false;
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_persist_load_metadata(zmk_usb_bridge_metadata_t *metadata) {
    if (metadata == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_has_metadata) {
        return ESP_ERR_NOT_FOUND;
    }

    *metadata = g_metadata;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_persist_store_metadata(const zmk_usb_bridge_metadata_t *metadata) {
    if (metadata == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    g_metadata = *metadata;
    g_has_metadata = true;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_persist_discard_metadata(void) {
    memset(&g_metadata, 0, sizeof(g_metadata));
    g_has_metadata = false;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_persist_erase_all(void) {
    return zmk_usb_bridge_persist_discard_metadata();
}
