#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

#include <stdbool.h>
#include <zephyr/bluetooth/bluetooth.h>

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_connect_peer(const bt_addr_le_t *addr);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_disconnect_active(uint8_t reason);
bool zmk_usb_bridge_ble_connection_is_busy(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_success(uint16_t conn_handle);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_failure(
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_security_ready(uint16_t conn_handle);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_security_failure(
    uint16_t conn_handle,
    int32_t status_code
);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_disconnected(
    uint16_t conn_handle,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
);
