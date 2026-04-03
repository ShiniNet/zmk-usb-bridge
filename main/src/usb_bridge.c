#include "zmk_usb_bridge/usb_bridge.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(zub_usb, LOG_LEVEL_INF);

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_init(void) {
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_keyboard(const zmk_usb_bridge_keyboard_body_t *body) {
    ARG_UNUSED(body);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_consumer(const zmk_usb_bridge_consumer_body_t *body) {
    ARG_UNUSED(body);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_mouse(const zmk_usb_bridge_mouse_body_t *body) {
    ARG_UNUSED(body);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_release_all(void) {
    LOG_INF("release_all");
    return ZMK_USB_BRIDGE_STATUS_OK;
}
