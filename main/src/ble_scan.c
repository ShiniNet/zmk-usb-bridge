#include "zmk_usb_bridge/ble_scan.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_scan, LOG_LEVEL_INF);

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_init(void) {
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_start_pairing(void) {
    LOG_INF("start pairing scan");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_start_known_device(void) {
    LOG_INF("start known-device scan");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_stop(void) {
    LOG_INF("stop");
    return ZMK_USB_BRIDGE_STATUS_OK;
}
