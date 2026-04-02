#pragma once

#include "zmk_usb_bridge/types.h"

#include <esp_err.h>

esp_err_t zmk_usb_bridge_state_machine_init(void);
esp_err_t zmk_usb_bridge_state_machine_start(void);
esp_err_t zmk_usb_bridge_state_machine_handle_event(const zmk_usb_bridge_event_t *event);
zmk_usb_bridge_state_t zmk_usb_bridge_state_machine_current_state(void);
