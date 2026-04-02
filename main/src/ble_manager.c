#include "zmk_usb_bridge/ble_manager.h"

#include "zmk_usb_bridge/hog_client.h"
#include "zmk_usb_bridge/state_machine.h"

#include <esp_log.h>

static const char *TAG = "zub_ble_mgr";

esp_err_t zmk_usb_bridge_ble_manager_init(void) {
    ESP_LOGI(TAG, "init");
    return zmk_usb_bridge_hog_client_init();
}

esp_err_t zmk_usb_bridge_ble_manager_start_scan(void) {
    ESP_LOGI(TAG, "start_scan");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_ble_manager_reset_fast_reconnect(void) {
    ESP_LOGI(TAG, "reset_fast_reconnect");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_ble_manager_on_gap_event(const struct ble_gap_event *event) {
    (void)event;
    ESP_LOGD(TAG, "gap_event");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_ble_manager_post_event(const zmk_usb_bridge_event_t *event) {
    return zmk_usb_bridge_state_machine_handle_event(event);
}
