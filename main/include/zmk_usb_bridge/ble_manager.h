#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_start_scan(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_reset_fast_reconnect(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_event(const zmk_usb_bridge_event_t *event);
