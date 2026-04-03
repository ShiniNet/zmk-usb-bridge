#include "zmk_usb_bridge/recovery_ui.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ui, LOG_LEVEL_INF);

static zmk_usb_bridge_state_t g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;

zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_init(void) {
    LOG_INF("init");
    g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_set_state(zmk_usb_bridge_state_t state) {
    g_ui_state = state;
    LOG_INF("ui_state=%d", state);
    return ZMK_USB_BRIDGE_STATUS_OK;
}
