#include "zmk_usb_bridge/ble_connection.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_conn, LOG_LEVEL_INF);

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_init(void) {
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}
