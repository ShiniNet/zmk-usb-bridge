#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

#include <stdbool.h>
#include <stdint.h>

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_reset(void);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_start_discovery(uint16_t conn_handle);
bool zmk_usb_bridge_hog_client_ready(void);
const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void);
