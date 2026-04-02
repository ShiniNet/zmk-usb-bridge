#pragma once

#include "zmk_usb_bridge/types.h"

#include <esp_err.h>

esp_err_t zmk_usb_bridge_persist_init(void);
esp_err_t zmk_usb_bridge_persist_load_metadata(zmk_usb_bridge_metadata_t *metadata);
esp_err_t zmk_usb_bridge_persist_store_metadata(const zmk_usb_bridge_metadata_t *metadata);
esp_err_t zmk_usb_bridge_persist_discard_metadata(void);
esp_err_t zmk_usb_bridge_persist_erase_all(void);
