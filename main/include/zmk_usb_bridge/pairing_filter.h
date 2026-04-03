#pragma once

#include <stdbool.h>
#include <esp_err.h>

typedef struct {
    bool connectable;
    bool has_hid_service;
    bool has_keyboard_appearance;
    const char *local_name;
} zmk_usb_bridge_pairing_candidate_t;

esp_err_t zmk_usb_bridge_pairing_filter_init(void);
bool zmk_usb_bridge_pairing_filter_name_allowlist_enabled(void);
bool zmk_usb_bridge_pairing_filter_name_allowed(const char *local_name);
bool zmk_usb_bridge_pairing_filter_accept_unbonded_candidate(
    const zmk_usb_bridge_pairing_candidate_t *candidate
);
