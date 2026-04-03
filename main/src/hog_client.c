#include "zmk_usb_bridge/hog_client.h"

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

bool zmk_usb_bridge_hog_client_ready(void) {
    return profile.has_keyboard_input && profile.has_consumer_input && profile.has_mouse_input;
}

const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void) {
    return &profile;
}
