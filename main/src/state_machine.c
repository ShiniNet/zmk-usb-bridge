#include "zmk_usb_bridge/state_machine.h"

#include "zmk_usb_bridge/bridge.h"
#include "zmk_usb_bridge/recovery_ui.h"

#include <esp_log.h>

static const char *TAG = "zub_sm";
static zmk_usb_bridge_state_t g_state = ZMK_USB_BRIDGE_STATE_BOOT;

static esp_err_t set_state(zmk_usb_bridge_state_t next_state) {
    g_state = next_state;
    ESP_LOGI(TAG, "state=%d", next_state);
    return zmk_usb_bridge_recovery_ui_set_state(next_state);
}

esp_err_t zmk_usb_bridge_state_machine_init(void) {
    ESP_ERROR_CHECK(zmk_usb_bridge_bridge_init());
    return set_state(ZMK_USB_BRIDGE_STATE_BOOT);
}

esp_err_t zmk_usb_bridge_state_machine_start(void) {
    return set_state(ZMK_USB_BRIDGE_STATE_USB_READY);
}

esp_err_t zmk_usb_bridge_state_machine_handle_event(const zmk_usb_bridge_event_t *event) {
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->type) {
    case ZMK_USB_BRIDGE_EVENT_USB_READY:
        return set_state(ZMK_USB_BRIDGE_STATE_USB_READY);
    case ZMK_USB_BRIDGE_EVENT_CONNECT_SUCCESS:
        return set_state(ZMK_USB_BRIDGE_STATE_CONNECTING);
    case ZMK_USB_BRIDGE_EVENT_HID_READY:
        return set_state(ZMK_USB_BRIDGE_STATE_CONNECTED);
    case ZMK_USB_BRIDGE_EVENT_DISCONNECTED:
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST);
    case ZMK_USB_BRIDGE_EVENT_BOND_MISMATCH:
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED);
    case ZMK_USB_BRIDGE_EVENT_BUTTON_LONG_PRESS:
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(ZMK_USB_BRIDGE_STATE_BOND_ERASING);
    default:
        return ESP_OK;
    }
}

zmk_usb_bridge_state_t zmk_usb_bridge_state_machine_current_state(void) {
    return g_state;
}
