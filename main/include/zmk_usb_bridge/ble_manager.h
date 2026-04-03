#pragma once

#include "zmk_usb_bridge/ble_command.h"
#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_start_scan(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_reset_fast_reconnect(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_execute_command(
    zmk_usb_bridge_ble_command_t command
);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_event(const zmk_usb_bridge_event_t *event);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_simple_event(
    zmk_usb_bridge_event_type_t type
);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_event_with_payload(
    zmk_usb_bridge_event_type_t type,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code,
    uint16_t conn_handle,
    uint16_t capability_flags
);
