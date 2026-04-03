#include "zmk_usb_bridge/bridge.h"

#include "zmk_usb_bridge/usb_bridge.h"

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_bridge, LOG_LEVEL_INF);

static zmk_usb_bridge_keyboard_body_t keyboard_state;
static zmk_usb_bridge_consumer_body_t consumer_state;
static zmk_usb_bridge_mouse_body_t mouse_state;

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_init(void) {
    return zmk_usb_bridge_bridge_reset_state();
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_reset_state(void) {
    memset(&keyboard_state, 0, sizeof(keyboard_state));
    memset(&consumer_state, 0, sizeof(consumer_state));
    memset(&mouse_state, 0, sizeof(mouse_state));
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_release_all(void) {
    zmk_usb_bridge_status_t status = zmk_usb_bridge_bridge_reset_state();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    return zmk_usb_bridge_usb_bridge_release_all();
}

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_handle_input(
    zmk_usb_bridge_report_role_t role,
    const void *payload,
    size_t payload_len
) {
    switch (role) {
    case ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT:
        if (payload_len != sizeof(keyboard_state)) {
            return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
        }
        memcpy(&keyboard_state, payload, sizeof(keyboard_state));
        return zmk_usb_bridge_usb_bridge_send_keyboard(&keyboard_state);
    case ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT:
        if (payload_len != sizeof(consumer_state)) {
            return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
        }
        memcpy(&consumer_state, payload, sizeof(consumer_state));
        return zmk_usb_bridge_usb_bridge_send_consumer(&consumer_state);
    case ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT:
        if (payload_len != sizeof(mouse_state)) {
            return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
        }
        memcpy(&mouse_state, payload, sizeof(mouse_state));
        return zmk_usb_bridge_usb_bridge_send_mouse(&mouse_state);
    default:
        LOG_WRN("ignoring unsupported role=%d", role);
        return ZMK_USB_BRIDGE_STATUS_OK;
    }
}
