#include "zmk_usb_bridge/state_machine.h"

#include "zmk_usb_bridge/bridge.h"
#include "zmk_usb_bridge/recovery_ui.h"

#include <esp_log.h>
#include <stdbool.h>

static const char *TAG = "zub_sm";
static zmk_usb_bridge_state_t g_state = ZMK_USB_BRIDGE_STATE_BOOT;
static bool g_usb_ready;
static bool g_ble_synced;
static bool g_persist_ready;
static bool g_has_bond;

static esp_err_t set_state(zmk_usb_bridge_state_t next_state) {
    g_state = next_state;
    ESP_LOGI(TAG, "state=%d", next_state);
    return zmk_usb_bridge_recovery_ui_set_state(next_state);
}

static esp_err_t advance_startup_gate(void) {
    if (!g_usb_ready) {
        return ESP_OK;
    }

    if (g_state == ZMK_USB_BRIDGE_STATE_BOOT) {
        ESP_ERROR_CHECK(set_state(ZMK_USB_BRIDGE_STATE_USB_READY));
    }

    if (!g_ble_synced || !g_persist_ready) {
        return ESP_OK;
    }

    if (g_state != ZMK_USB_BRIDGE_STATE_USB_READY) {
        return ESP_OK;
    }

    return set_state(g_has_bond ? ZMK_USB_BRIDGE_STATE_SCANNING_KNOWN_DEVICE
                                : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN);
}

esp_err_t zmk_usb_bridge_state_machine_init(void) {
    ESP_ERROR_CHECK(zmk_usb_bridge_bridge_init());
    g_usb_ready = false;
    g_ble_synced = false;
    g_persist_ready = false;
    g_has_bond = false;
    return set_state(ZMK_USB_BRIDGE_STATE_BOOT);
}

esp_err_t zmk_usb_bridge_state_machine_start(void) {
    const zmk_usb_bridge_event_t event = {
        .type = ZMK_USB_BRIDGE_EVENT_USB_READY,
    };
    return zmk_usb_bridge_state_machine_handle_event(&event);
}

esp_err_t zmk_usb_bridge_state_machine_handle_event(const zmk_usb_bridge_event_t *event) {
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->type) {
    case ZMK_USB_BRIDGE_EVENT_USB_READY:
        g_usb_ready = true;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_BLE_SYNCED:
        g_ble_synced = true;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_PERSIST_READY_WITH_BOND:
        g_persist_ready = true;
        g_has_bond = true;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_PERSIST_READY_NO_BOND:
        g_persist_ready = true;
        g_has_bond = false;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_KNOWN_DEVICE_FOUND:
    case ZMK_USB_BRIDGE_EVENT_PAIRING_CANDIDATE_FOUND:
    case ZMK_USB_BRIDGE_EVENT_CONNECT_SUCCESS:
        return set_state(ZMK_USB_BRIDGE_STATE_CONNECTING);
    case ZMK_USB_BRIDGE_EVENT_CONNECT_FAILURE:
    case ZMK_USB_BRIDGE_EVENT_HID_FAILURE:
        return set_state(g_has_bond ? ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST
                                    : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN);
    case ZMK_USB_BRIDGE_EVENT_HID_READY:
        return set_state(ZMK_USB_BRIDGE_STATE_CONNECTED);
    case ZMK_USB_BRIDGE_EVENT_DISCONNECTED:
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(g_has_bond ? ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST
                                    : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN);
    case ZMK_USB_BRIDGE_EVENT_HOST_RESET:
        g_ble_synced = false;
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(ZMK_USB_BRIDGE_STATE_USB_READY);
    case ZMK_USB_BRIDGE_EVENT_BOND_MISMATCH:
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED);
    case ZMK_USB_BRIDGE_EVENT_METADATA_FAULT:
        return set_state(g_has_bond ? ZMK_USB_BRIDGE_STATE_SCANNING_KNOWN_DEVICE
                                    : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN);
    case ZMK_USB_BRIDGE_EVENT_BUTTON_SHORT_PRESS:
        if (g_state == ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED) {
            return ESP_OK;
        }
        return set_state(g_has_bond ? ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST
                                    : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN);
    case ZMK_USB_BRIDGE_EVENT_BUTTON_LONG_PRESS:
        ESP_ERROR_CHECK(zmk_usb_bridge_bridge_release_all());
        return set_state(ZMK_USB_BRIDGE_STATE_BOND_ERASING);
    case ZMK_USB_BRIDGE_EVENT_BOND_ERASE_COMPLETE:
        g_has_bond = false;
        g_persist_ready = true;
        return set_state(ZMK_USB_BRIDGE_STATE_PAIRING_SCAN);
    default:
        return ESP_OK;
    }
}

zmk_usb_bridge_state_t zmk_usb_bridge_state_machine_current_state(void) {
    return g_state;
}
