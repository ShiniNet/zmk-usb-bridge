#pragma once

#include "zmk_usb_bridge/bridge.h"

#include <esp_err.h>

esp_err_t zmk_usb_bridge_usb_bridge_init(void);
esp_err_t zmk_usb_bridge_usb_bridge_send_keyboard(const zmk_usb_bridge_keyboard_body_t *body);
esp_err_t zmk_usb_bridge_usb_bridge_send_consumer(const zmk_usb_bridge_consumer_body_t *body);
esp_err_t zmk_usb_bridge_usb_bridge_send_mouse(const zmk_usb_bridge_mouse_body_t *body);
esp_err_t zmk_usb_bridge_usb_bridge_release_all(void);
