#include "zmk_usb_bridge/bridge.h"

#include "zmk_usb_bridge/usb_bridge.h"

#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "zub_bridge";

static zmk_usb_bridge_keyboard_body_t keyboard_state;
static zmk_usb_bridge_consumer_body_t consumer_state;
static zmk_usb_bridge_mouse_body_t mouse_state;

esp_err_t zmk_usb_bridge_bridge_init(void) {
    return zmk_usb_bridge_bridge_reset_state();
}

esp_err_t zmk_usb_bridge_bridge_reset_state(void) {
    memset(&keyboard_state, 0, sizeof(keyboard_state));
    memset(&consumer_state, 0, sizeof(consumer_state));
    memset(&mouse_state, 0, sizeof(mouse_state));
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_bridge_release_all(void) {
    ESP_ERROR_CHECK(zmk_usb_bridge_bridge_reset_state());
    return zmk_usb_bridge_usb_bridge_release_all();
}

esp_err_t zmk_usb_bridge_bridge_handle_input(
    zmk_usb_bridge_report_role_t role,
    const void *payload,
    size_t payload_len
) {
    switch (role) {
    case ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT:
        if (payload_len != sizeof(keyboard_state)) {
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(&keyboard_state, payload, sizeof(keyboard_state));
        return zmk_usb_bridge_usb_bridge_send_keyboard(&keyboard_state);
    case ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT:
        if (payload_len != sizeof(consumer_state)) {
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(&consumer_state, payload, sizeof(consumer_state));
        return zmk_usb_bridge_usb_bridge_send_consumer(&consumer_state);
    case ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT:
        if (payload_len != sizeof(mouse_state)) {
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(&mouse_state, payload, sizeof(mouse_state));
        return zmk_usb_bridge_usb_bridge_send_mouse(&mouse_state);
    default:
        ESP_LOGW(TAG, "ignoring unsupported role=%d", role);
        return ESP_OK;
    }
}
