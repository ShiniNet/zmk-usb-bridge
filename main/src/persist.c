#include "zmk_usb_bridge/persist.h"

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_persist, LOG_LEVEL_INF);

static zmk_usb_bridge_metadata_t g_metadata;
static bool g_has_metadata;

zmk_usb_bridge_status_t zmk_usb_bridge_persist_init(void) {
    memset(&g_metadata, 0, sizeof(g_metadata));
    g_has_metadata = false;
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_persist_load_metadata(zmk_usb_bridge_metadata_t *metadata) {
    if (metadata == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    if (!g_has_metadata) {
        return ZMK_USB_BRIDGE_STATUS_NOT_FOUND;
    }

    *metadata = g_metadata;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_persist_store_metadata(const zmk_usb_bridge_metadata_t *metadata) {
    if (metadata == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    g_metadata = *metadata;
    g_has_metadata = true;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_persist_discard_metadata(void) {
    memset(&g_metadata, 0, sizeof(g_metadata));
    g_has_metadata = false;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_persist_erase_all(void) {
    return zmk_usb_bridge_persist_discard_metadata();
}
