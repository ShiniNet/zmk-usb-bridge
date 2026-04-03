#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

zmk_usb_bridge_status_t zmk_usb_bridge_state_machine_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_state_machine_start(void);
zmk_usb_bridge_status_t zmk_usb_bridge_state_machine_handle_event(const zmk_usb_bridge_event_t *event);
zmk_usb_bridge_state_t zmk_usb_bridge_state_machine_current_state(void);
