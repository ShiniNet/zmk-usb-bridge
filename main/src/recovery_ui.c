#include "zmk_usb_bridge/recovery_ui.h"

#include <esp_log.h>

static const char *TAG = "zub_ui";
static zmk_usb_bridge_state_t g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;

esp_err_t zmk_usb_bridge_recovery_ui_init(void) {
    ESP_LOGI(TAG, "init");
    g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_recovery_ui_set_state(zmk_usb_bridge_state_t state) {
    g_ui_state = state;
    ESP_LOGI(TAG, "ui_state=%d", state);
    return ESP_OK;
}
