#include "zmk_usb_bridge/hog_client.h"

#include "zmk_usb_bridge/ble_manager.h"

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_hog, LOG_LEVEL_INF);

static zmk_usb_bridge_hog_profile_t profile;

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_init(void) {
    memset(&profile, 0, sizeof(profile));
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_reset(void) {
    memset(&profile, 0, sizeof(profile));
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_start_discovery(uint16_t conn_handle) {
    LOG_INF("start_discovery conn_handle=%u", conn_handle);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_set_profile(
    const zmk_usb_bridge_hog_profile_t *next_profile
) {
    if (next_profile == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    profile = *next_profile;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_complete_discovery(uint16_t conn_handle) {
    if (!zmk_usb_bridge_hog_client_ready()) {
        return zmk_usb_bridge_ble_manager_post_event_with_payload(
            ZMK_USB_BRIDGE_EVENT_HID_FAILURE,
            ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
            ZMK_USB_BRIDGE_STATUS_NOT_FOUND,
            conn_handle,
            zmk_usb_bridge_hog_client_capability_flags()
        );
    }

    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_HID_READY,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        conn_handle,
        zmk_usb_bridge_hog_client_capability_flags()
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_fail_discovery(
    uint16_t conn_handle,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
) {
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_HID_FAILURE,
        reason,
        status_code,
        conn_handle,
        zmk_usb_bridge_hog_client_capability_flags()
    );
}

bool zmk_usb_bridge_hog_client_ready(void) {
    return profile.has_keyboard_input;
}

uint16_t zmk_usb_bridge_hog_client_capability_flags(void) {
    uint16_t flags = ZMK_USB_BRIDGE_CAPABILITY_NONE;

    if (profile.has_keyboard_input) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_KEYBOARD_INPUT;
    }

    if (profile.has_consumer_input) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_CONSUMER_INPUT;
    }

    if (profile.has_mouse_input) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_MOUSE_INPUT;
    }

    if (profile.has_mouse_feature) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_MOUSE_FEATURE;
    }

    if (profile.has_led_output) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_LED_OUTPUT;
    }

    return flags;
}

const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void) {
    return &profile;
}
