#include "zmk_usb_bridge/ble_connection.h"

#include "zmk_usb_bridge/ble_manager.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_conn, LOG_LEVEL_INF);

static uint16_t g_active_conn_handle;

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_init(void) {
    g_active_conn_handle = 0;
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_success(uint16_t conn_handle) {
    g_active_conn_handle = conn_handle;
    LOG_INF("connect success conn_handle=%u", conn_handle);
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_CONNECT_SUCCESS,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_failure(
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
) {
    LOG_INF("connect failure reason=%d status_code=%d", reason, (int)status_code);
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_CONNECT_FAILURE,
        reason,
        status_code,
        0,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_security_ready(uint16_t conn_handle) {
    LOG_INF("security ready conn_handle=%u", conn_handle);
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_SECURITY_READY,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_security_failure(
    uint16_t conn_handle,
    int32_t status_code
) {
    LOG_INF("security failure conn_handle=%u status_code=%d", conn_handle, (int)status_code);
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_CONNECT_FAILURE,
        ZMK_USB_BRIDGE_EVENT_REASON_SECURITY_FAILED,
        status_code,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_disconnected(
    uint16_t conn_handle,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
) {
    if (g_active_conn_handle == conn_handle) {
        g_active_conn_handle = 0;
    }

    LOG_INF(
        "disconnected conn_handle=%u reason=%d status_code=%d",
        conn_handle,
        reason,
        (int)status_code
    );
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_DISCONNECTED,
        reason,
        status_code,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}
