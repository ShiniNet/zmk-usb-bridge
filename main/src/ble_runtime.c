#include "zmk_usb_bridge/ble_runtime.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_rt, LOG_LEVEL_INF);

zmk_usb_bridge_status_t zmk_usb_bridge_ble_runtime_init(void) {
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_runtime_erase_bonds(void) {
    LOG_INF("erase bonds");
    return ZMK_USB_BRIDGE_STATUS_OK;
}
