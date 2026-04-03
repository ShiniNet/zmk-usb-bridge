#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} zmk_usb_bridge_keyboard_body_t;

typedef struct {
    uint16_t usage;
} zmk_usb_bridge_consumer_body_t;

typedef struct {
    uint8_t buttons;
    int16_t dx;
    int16_t dy;
    int16_t scroll_y;
    int16_t scroll_x;
} zmk_usb_bridge_mouse_body_t;

zmk_usb_bridge_status_t zmk_usb_bridge_bridge_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_bridge_reset_state(void);
zmk_usb_bridge_status_t zmk_usb_bridge_bridge_release_all(void);
zmk_usb_bridge_status_t zmk_usb_bridge_bridge_handle_input(
    zmk_usb_bridge_report_role_t role,
    const void *payload,
    size_t payload_len
);
