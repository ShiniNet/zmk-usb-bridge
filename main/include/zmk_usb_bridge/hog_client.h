#pragma once

#include "zmk_usb_bridge/types.h"

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

struct ble_gap_event;

esp_err_t zmk_usb_bridge_hog_client_init(void);
esp_err_t zmk_usb_bridge_hog_client_reset(void);
esp_err_t zmk_usb_bridge_hog_client_start_discovery(uint16_t conn_handle);
esp_err_t zmk_usb_bridge_hog_client_on_gap_event(const struct ble_gap_event *event);
bool zmk_usb_bridge_hog_client_ready(void);
const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void);
