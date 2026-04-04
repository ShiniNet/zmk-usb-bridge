#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/bluetooth/conn.h>

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_reset(void);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_start_discovery(
    struct bt_conn *conn,
    uint16_t conn_handle
);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_set_profile(
    const zmk_usb_bridge_hog_profile_t *next_profile
);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_complete_discovery(uint16_t conn_handle);
zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_fail_discovery(
    uint16_t conn_handle,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
);
bool zmk_usb_bridge_hog_client_discovery_active(uint16_t conn_handle);
bool zmk_usb_bridge_hog_client_ready(void);
uint16_t zmk_usb_bridge_hog_client_capability_flags(void);
const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void);
