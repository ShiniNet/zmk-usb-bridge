#pragma once

#include "zmk_usb_bridge/status.h"

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_start_pairing(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_start_known_device(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_stop(void);
