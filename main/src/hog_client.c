#include "zmk_usb_bridge/hog_client.h"

#include <esp_log.h>
#include <string.h>

static const char *TAG = "zub_hog";

static zmk_usb_bridge_hog_profile_t profile;

esp_err_t zmk_usb_bridge_hog_client_init(void) {
    memset(&profile, 0, sizeof(profile));
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_hog_client_reset(void) {
    memset(&profile, 0, sizeof(profile));
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_hog_client_start_discovery(uint16_t conn_handle) {
    ESP_LOGI(TAG, "start_discovery conn_handle=%u", conn_handle);
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_hog_client_on_gap_event(const struct ble_gap_event *event) {
    (void)event;
    return ESP_OK;
}

bool zmk_usb_bridge_hog_client_ready(void) {
    return profile.has_keyboard_input && profile.has_consumer_input && profile.has_mouse_input;
}

const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void) {
    return &profile;
}
