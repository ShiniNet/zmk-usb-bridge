#include "zmk_usb_bridge/persist.h"

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_persist, LOG_LEVEL_INF);

enum {
    ZMK_USB_BRIDGE_METADATA_VERSION_CURRENT = 1,
};

static zmk_usb_bridge_metadata_t g_metadata;
static bool g_has_metadata;

static bool metadata_version_supported(const zmk_usb_bridge_metadata_t *metadata) {
    return metadata != NULL && metadata->metadata_version == ZMK_USB_BRIDGE_METADATA_VERSION_CURRENT;
}

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

    if (!metadata_version_supported(&g_metadata)) {
        LOG_WRN(
            "metadata version mismatch stored=%u expected=%u",
            g_metadata.metadata_version,
            ZMK_USB_BRIDGE_METADATA_VERSION_CURRENT
        );
        return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
    }

    *metadata = g_metadata;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_persist_store_metadata(const zmk_usb_bridge_metadata_t *metadata) {
    if (metadata == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    g_metadata = *metadata;
    g_metadata.metadata_version = ZMK_USB_BRIDGE_METADATA_VERSION_CURRENT;
    g_has_metadata = true;
    LOG_INF(
        "metadata stored version=%u last_peer_valid=%d identity_valid=%d",
        g_metadata.metadata_version,
        g_metadata.last_peer_snapshot.valid,
        g_metadata.identity_snapshot.valid
    );
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
