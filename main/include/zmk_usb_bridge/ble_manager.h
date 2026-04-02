#pragma once

#include "zmk_usb_bridge/types.h"

#include <esp_err.h>

struct ble_gap_event;

esp_err_t zmk_usb_bridge_ble_manager_init(void);
esp_err_t zmk_usb_bridge_ble_manager_start_scan(void);
esp_err_t zmk_usb_bridge_ble_manager_reset_fast_reconnect(void);
esp_err_t zmk_usb_bridge_ble_manager_on_gap_event(const struct ble_gap_event *event);
esp_err_t zmk_usb_bridge_ble_manager_post_event(const zmk_usb_bridge_event_t *event);
