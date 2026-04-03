#include "zmk_usb_bridge/ble_reconnect.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_reconn, LOG_LEVEL_INF);

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_init(void) {
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_enter_fast_mode(void) {
    LOG_INF("enter fast reconnect mode");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_enter_backoff_mode(void) {
    LOG_INF("enter backoff reconnect mode");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_reset_fast_reconnect(void) {
    LOG_INF("reset_fast_reconnect");
    return ZMK_USB_BRIDGE_STATUS_OK;
}
