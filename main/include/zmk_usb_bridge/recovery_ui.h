#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_enable_inputs(void);
zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_set_state(zmk_usb_bridge_state_t state);
