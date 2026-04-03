#pragma once

#include "zmk_usb_bridge/status.h"

#include <stdbool.h>
#include <stddef.h>

zmk_usb_bridge_status_t zmk_usb_bridge_ble_runtime_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_ble_runtime_erase_bonds(void);
bool zmk_usb_bridge_ble_runtime_is_ready(void);
bool zmk_usb_bridge_ble_runtime_has_bond(void);
size_t zmk_usb_bridge_ble_runtime_bond_count(void);
