#include "zmk_usb_bridge/usb_bridge.h"

#include <esp_log.h>

static const char *TAG = "zub_usb";

esp_err_t zmk_usb_bridge_usb_bridge_init(void) {
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_usb_bridge_send_keyboard(const zmk_usb_bridge_keyboard_body_t *body) {
    (void)body;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_usb_bridge_send_consumer(const zmk_usb_bridge_consumer_body_t *body) {
    (void)body;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_usb_bridge_send_mouse(const zmk_usb_bridge_mouse_body_t *body) {
    (void)body;
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_usb_bridge_release_all(void) {
    ESP_LOGI(TAG, "release_all");
    return ESP_OK;
}
