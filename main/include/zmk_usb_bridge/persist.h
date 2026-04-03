#pragma once

#include "zmk_usb_bridge/status.h"
#include "zmk_usb_bridge/types.h"

zmk_usb_bridge_status_t zmk_usb_bridge_persist_init(void);
zmk_usb_bridge_status_t zmk_usb_bridge_persist_load_metadata(zmk_usb_bridge_metadata_t *metadata);
zmk_usb_bridge_status_t zmk_usb_bridge_persist_store_metadata(const zmk_usb_bridge_metadata_t *metadata);
zmk_usb_bridge_status_t zmk_usb_bridge_persist_discard_metadata(void);
zmk_usb_bridge_status_t zmk_usb_bridge_persist_erase_all(void);
