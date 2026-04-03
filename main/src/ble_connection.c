#include "zmk_usb_bridge/ble_connection.h"

#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/hog_client.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_conn, LOG_LEVEL_INF);

static uint16_t g_active_conn_handle;
static bool g_callbacks_registered;
static struct bt_conn *g_active_conn;

static uint16_t conn_handle_of(const struct bt_conn *conn) {
    if (conn == NULL) {
        return 0;
    }

    return (uint16_t)bt_conn_index(conn) + 1U;
}

static zmk_usb_bridge_event_reason_t disconnect_reason_from_hci(uint8_t reason) {
    switch (reason) {
    case BT_HCI_ERR_CONN_TIMEOUT:
        return ZMK_USB_BRIDGE_EVENT_REASON_DISCONNECTED_TIMEOUT;
    default:
        return ZMK_USB_BRIDGE_EVENT_REASON_DISCONNECTED_REMOTE;
    }
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    const uint16_t conn_handle = conn_handle_of(conn);

    if (err != 0U) {
        if (g_active_conn != NULL) {
            bt_conn_unref(g_active_conn);
            g_active_conn = NULL;
        }

        (void)zmk_usb_bridge_ble_connection_on_connect_failure(
            ZMK_USB_BRIDGE_EVENT_REASON_CONNECT_CREATE_FAILED,
            err
        );
        return;
    }

    (void)zmk_usb_bridge_ble_connection_on_connect_success(conn_handle);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    const uint16_t conn_handle = conn_handle_of(conn);

    (void)zmk_usb_bridge_ble_connection_on_disconnected(
        conn_handle,
        disconnect_reason_from_hci(reason),
        reason
    );
}

static void on_security_changed(
    struct bt_conn *conn,
    bt_security_t level,
    enum bt_security_err err
) {
    const uint16_t conn_handle = conn_handle_of(conn);

    ARG_UNUSED(level);

    if (err == BT_SECURITY_ERR_SUCCESS) {
        (void)zmk_usb_bridge_ble_connection_on_security_ready(conn_handle);
        return;
    }

    (void)zmk_usb_bridge_ble_connection_on_security_failure(conn_handle, err);
}

static struct bt_conn_cb g_conn_callbacks = {
    .connected = on_connected,
    .disconnected = on_disconnected,
    .security_changed = on_security_changed,
};

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_init(void) {
    g_active_conn_handle = 0;
    g_active_conn = NULL;

    if (!g_callbacks_registered) {
        bt_conn_cb_register(&g_conn_callbacks);
        g_callbacks_registered = true;
    }

    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_connect_peer(const bt_addr_le_t *addr) {
    int err;
    struct bt_conn *conn = NULL;

    if (addr == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    if (g_active_conn != NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
    if (err != 0) {
        LOG_INF("connect create failed err=%d", err);
        return zmk_usb_bridge_ble_connection_on_connect_failure(
            ZMK_USB_BRIDGE_EVENT_REASON_CONNECT_CREATE_FAILED,
            err
        );
    }

    g_active_conn = conn;
    LOG_INF("connect create started");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_disconnect_active(uint8_t reason) {
    int err;

    if (g_active_conn == NULL) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    err = bt_conn_disconnect(g_active_conn, reason);
    if (err != 0) {
        LOG_WRN("disconnect active failed err=%d reason=0x%02x", err, reason);
        return (zmk_usb_bridge_status_t)err;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_success(uint16_t conn_handle) {
    int err;

    g_active_conn_handle = conn_handle;
    LOG_INF("connect success conn_handle=%u", conn_handle);
    err = zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_CONNECT_SUCCESS,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
    if (err != ZMK_USB_BRIDGE_STATUS_OK) {
        return (zmk_usb_bridge_status_t)err;
    }

    if (g_active_conn == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    err = bt_conn_set_security(g_active_conn, BT_SECURITY_L2);
    if (err != 0) {
        return zmk_usb_bridge_ble_connection_on_security_failure(conn_handle, err);
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
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
    zmk_usb_bridge_status_t status;

    LOG_INF("security ready conn_handle=%u", conn_handle);
    status = zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_SECURITY_READY,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    return zmk_usb_bridge_hog_client_start_discovery(g_active_conn, conn_handle);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_security_failure(
    uint16_t conn_handle,
    int32_t status_code
) {
    if (g_active_conn != NULL) {
        (void)bt_conn_disconnect(g_active_conn, BT_HCI_ERR_AUTH_FAIL);
    }

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
    (void)zmk_usb_bridge_hog_client_reset();

    if (g_active_conn != NULL) {
        bt_conn_unref(g_active_conn);
        g_active_conn = NULL;
    }

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
