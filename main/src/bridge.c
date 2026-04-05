#include "zmk_usb_bridge/bridge.h"

#include "zmk_usb_bridge/usb_bridge.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_bridge, LOG_LEVEL_INF);

static zmk_usb_bridge_keyboard_body_t keyboard_state;
static zmk_usb_bridge_consumer_body_t consumer_state;
static zmk_usb_bridge_mouse_body_t mouse_state;
static bool g_input_active;
static K_MUTEX_DEFINE(g_bridge_lock);

static void reset_state_locked(void) {
    memset(&keyboard_state, 0, sizeof(keyboard_state));
    memset(&consumer_state, 0, sizeof(consumer_state));
    memset(&mouse_state, 0, sizeof(mouse_state));
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_init(void) {
    k_mutex_lock(&g_bridge_lock, K_FOREVER);
    reset_state_locked();
    g_input_active = false;
    k_mutex_unlock(&g_bridge_lock);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_reset_state(void) {
    k_mutex_lock(&g_bridge_lock, K_FOREVER);
    reset_state_locked();
    k_mutex_unlock(&g_bridge_lock);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_set_input_active(bool active) {
    k_mutex_lock(&g_bridge_lock, K_FOREVER);
    g_input_active = active;
    if (!active) {
        reset_state_locked();
    }
    k_mutex_unlock(&g_bridge_lock);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_release_all(void) {
    k_mutex_lock(&g_bridge_lock, K_FOREVER);
    g_input_active = false;
    reset_state_locked();
    k_mutex_unlock(&g_bridge_lock);

    return zmk_usb_bridge_usb_bridge_release_all();
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_handle_input(
    zmk_usb_bridge_report_role_t role,
    const void *payload,
    size_t payload_len
) {
    zmk_usb_bridge_status_t status;

    if (payload == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    k_mutex_lock(&g_bridge_lock, K_FOREVER);
    if (!g_input_active) {
        k_mutex_unlock(&g_bridge_lock);
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    switch (role) {
    case ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT:
        if (payload_len != sizeof(keyboard_state)) {
            status = ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
            break;
        }
        memcpy(&keyboard_state, payload, sizeof(keyboard_state));
        status = zmk_usb_bridge_usb_bridge_send_keyboard(&keyboard_state);
        break;
    case ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT:
        if (payload_len != sizeof(consumer_state)) {
            status = ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
            break;
        }
        memcpy(&consumer_state, payload, sizeof(consumer_state));
        status = zmk_usb_bridge_usb_bridge_send_consumer(&consumer_state);
        break;
    case ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT:
        if (payload_len != sizeof(mouse_state)) {
            status = ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
            break;
        }
        memcpy(&mouse_state, payload, sizeof(mouse_state));
        status = zmk_usb_bridge_usb_bridge_send_mouse(&mouse_state);
        break;
    default:
        LOG_WRN("ignoring unsupported role=%d", role);
        status = ZMK_USB_BRIDGE_STATUS_OK;
        break;
    }

    k_mutex_unlock(&g_bridge_lock);
    return status;
}
