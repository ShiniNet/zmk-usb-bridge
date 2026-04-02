#pragma once

#include "zmk_usb_bridge/types.h"

#include <esp_err.h>

esp_err_t zmk_usb_bridge_recovery_ui_init(void);
esp_err_t zmk_usb_bridge_recovery_ui_set_state(zmk_usb_bridge_state_t state);
