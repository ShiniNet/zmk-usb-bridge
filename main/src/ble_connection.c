#include "zmk_usb_bridge/ble_connection.h"

#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/ble_reconnect.h"
#include "zmk_usb_bridge/ble_runtime.h"
#include "zmk_usb_bridge/hog_client.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_conn, LOG_LEVEL_INF);

enum {
    ZMK_USB_BRIDGE_BOND_MISMATCH_CONFIRMATION_COUNT = 2,
    ZMK_USB_BRIDGE_CONNECT_ATTEMPT_TIMEOUT_MS = 4000,
};

static uint16_t g_active_conn_handle;
static bool g_callbacks_registered;
static struct bt_conn *g_active_conn;
static bool g_security_ready;
static uint8_t g_bond_mismatch_streak;
static bool g_connect_attempt_pending;
static struct k_work_delayable g_connect_timeout_work;

static const struct bt_conn_le_create_param g_create_param = {
    .options = BT_CONN_LE_OPT_NONE,
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .window = BT_GAP_SCAN_FAST_INTERVAL,
    .interval_coded = 0,
    .window_coded = 0,
    .timeout = ZMK_USB_BRIDGE_CONNECT_ATTEMPT_TIMEOUT_MS / 10,
};

static void clear_connect_attempt_timeout(void) {
    g_connect_attempt_pending = false;
    (void)k_work_cancel_delayable(&g_connect_timeout_work);
}

static void connect_timeout_work_handler(struct k_work *work) {
    int err;

    ARG_UNUSED(work);

    if (!g_connect_attempt_pending || g_active_conn == NULL) {
        return;
    }

    g_connect_attempt_pending = false;
    LOG_WRN(
        "connect attempt watchdog expired timeout_ms=%d",
        ZMK_USB_BRIDGE_CONNECT_ATTEMPT_TIMEOUT_MS
    );

    err = bt_conn_disconnect(g_active_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err != 0) {
        LOG_WRN("connect attempt cancel failed err=%d", err);
    }
}

static zmk_usb_bridge_status_t request_link_security(uint16_t conn_handle) {
    int err;

    if (g_active_conn == NULL || g_active_conn_handle != conn_handle) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (g_security_ready) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    if (bt_conn_get_security(g_active_conn) >= BT_SECURITY_L2) {
        return zmk_usb_bridge_ble_connection_on_security_ready(conn_handle);
    }

    err = bt_conn_set_security(g_active_conn, BT_SECURITY_L2);
    if (err != 0) {
        return zmk_usb_bridge_ble_connection_on_security_failure(conn_handle, err);
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

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

static const char *security_err_name(enum bt_security_err err) {
    switch (err) {
    case BT_SECURITY_ERR_SUCCESS:
        return "success";
    case BT_SECURITY_ERR_AUTH_FAIL:
        return "auth_fail";
    case BT_SECURITY_ERR_PIN_OR_KEY_MISSING:
        return "pin_or_key_missing";
    case BT_SECURITY_ERR_OOB_NOT_AVAILABLE:
        return "oob_not_available";
    case BT_SECURITY_ERR_AUTH_REQUIREMENT:
        return "auth_requirement";
    case BT_SECURITY_ERR_PAIR_NOT_SUPPORTED:
        return "pair_not_supported";
    case BT_SECURITY_ERR_PAIR_NOT_ALLOWED:
        return "pair_not_allowed";
    case BT_SECURITY_ERR_INVALID_PARAM:
        return "invalid_param";
    case BT_SECURITY_ERR_KEY_REJECTED:
        return "key_rejected";
    case BT_SECURITY_ERR_UNSPECIFIED:
        return "unspecified";
    default:
        return "unknown";
    }
}

static const char *hci_err_name(uint8_t err) {
    switch (err) {
    case BT_HCI_ERR_SUCCESS:
        return "success";
    case BT_HCI_ERR_AUTH_FAIL:
        return "auth_fail";
    case BT_HCI_ERR_PIN_OR_KEY_MISSING:
        return "pin_or_key_missing";
    case BT_HCI_ERR_PAIRING_NOT_SUPPORTED:
        return "pairing_not_supported";
    case BT_HCI_ERR_PAIRING_NOT_ALLOWED:
        return "pairing_not_allowed";
    case BT_HCI_ERR_CONN_TIMEOUT:
        return "conn_timeout";
    case BT_HCI_ERR_CONN_FAIL_TO_ESTAB:
        return "conn_fail_to_estab";
    case BT_HCI_ERR_REMOTE_USER_TERM_CONN:
        return "remote_user_term_conn";
    case BT_HCI_ERR_LOCALHOST_TERM_CONN:
        return "localhost_term_conn";
    case BT_HCI_ERR_UNSPECIFIED:
        return "unspecified";
    default:
        return "unknown";
    }
}

static bool security_failure_looks_like_bond_mismatch(enum bt_security_err err) {
    if (!zmk_usb_bridge_ble_runtime_has_bond()) {
        return false;
    }

    switch (err) {
    case BT_SECURITY_ERR_AUTH_FAIL:
    case BT_SECURITY_ERR_PIN_OR_KEY_MISSING:
    case BT_SECURITY_ERR_UNSPECIFIED:
        return true;
    default:
        return false;
    }
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    const uint16_t conn_handle = conn_handle_of(conn);

    clear_connect_attempt_timeout();

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
    const bt_addr_le_t *dst = bt_conn_get_dst(conn);
    const bool bonded = zmk_usb_bridge_ble_runtime_is_peer_bonded(dst);

    if (err == BT_SECURITY_ERR_SUCCESS) {
        LOG_INF("security level=%u bonded=%d", level, bonded);
        (void)zmk_usb_bridge_ble_connection_on_security_ready(conn_handle);
        return;
    }

    LOG_WRN(
        "security changed failed level=%u bonded=%d err=%d name=%s",
        level,
        bonded,
        err,
        security_err_name(err)
    );
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
    g_security_ready = false;
    g_bond_mismatch_streak = 0;
    g_connect_attempt_pending = false;
    k_work_init_delayable(&g_connect_timeout_work, connect_timeout_work_handler);

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

    err = bt_conn_le_create(addr, &g_create_param, BT_LE_CONN_PARAM_DEFAULT, &conn);
    if (err != 0) {
        LOG_INF("connect create failed err=%d", err);
        return zmk_usb_bridge_ble_connection_on_connect_failure(
            ZMK_USB_BRIDGE_EVENT_REASON_CONNECT_CREATE_FAILED,
            err
        );
    }

    g_active_conn = conn;
    g_connect_attempt_pending = true;
    k_work_reschedule(
        &g_connect_timeout_work,
        K_MSEC(ZMK_USB_BRIDGE_CONNECT_ATTEMPT_TIMEOUT_MS)
    );
    LOG_INF(
        "connect create started timeout_ms=%d",
        ZMK_USB_BRIDGE_CONNECT_ATTEMPT_TIMEOUT_MS
    );
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

bool zmk_usb_bridge_ble_connection_is_busy(void) {
    return g_active_conn != NULL;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_success(uint16_t conn_handle) {
    zmk_usb_bridge_status_t err;
    const bt_addr_le_t *dst;
    bool bonded = false;

    clear_connect_attempt_timeout();
    g_active_conn_handle = conn_handle;
    g_security_ready = false;
    if (g_active_conn != NULL) {
        dst = bt_conn_get_dst(g_active_conn);
        bonded = zmk_usb_bridge_ble_runtime_is_peer_bonded(dst);
    }

    LOG_INF("connect success conn_handle=%u bonded=%d", conn_handle, bonded);
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

    if (bonded) {
        LOG_INF("bonded peer -> start discovery without explicit security conn_handle=%u",
                conn_handle);
        return zmk_usb_bridge_hog_client_start_discovery(g_active_conn, conn_handle);
    }

    return request_link_security(conn_handle);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_connection_on_connect_failure(
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
) {
    clear_connect_attempt_timeout();
    (void)zmk_usb_bridge_ble_reconnect_note_failure();
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

    if (g_active_conn_handle == conn_handle && g_security_ready) {
        LOG_INF("security ready duplicate ignored conn_handle=%u", conn_handle);
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    g_security_ready = true;
    g_bond_mismatch_streak = 0;
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
    const enum bt_security_err err = (enum bt_security_err)status_code;
    const bool bond_mismatch = security_failure_looks_like_bond_mismatch(err);
    bool confirmed_bond_mismatch = false;

    g_security_ready = false;

    if (bond_mismatch) {
        if (g_bond_mismatch_streak < UINT8_MAX) {
            g_bond_mismatch_streak++;
        }
        confirmed_bond_mismatch =
            g_bond_mismatch_streak >= ZMK_USB_BRIDGE_BOND_MISMATCH_CONFIRMATION_COUNT;
    } else {
        g_bond_mismatch_streak = 0;
    }

    if (!bond_mismatch || !confirmed_bond_mismatch) {
        (void)zmk_usb_bridge_ble_reconnect_note_failure();
    }

    if (g_active_conn != NULL) {
        (void)bt_conn_disconnect(g_active_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    LOG_WRN(
        "security failure conn_handle=%u status_code=%d name=%s hci_name=%s bond_mismatch=%d streak=%u confirmed=%d",
        conn_handle,
        (int)status_code,
        security_err_name(err),
        hci_err_name((uint8_t)status_code),
        bond_mismatch,
        g_bond_mismatch_streak,
        confirmed_bond_mismatch
    );

    if (confirmed_bond_mismatch) {
        return zmk_usb_bridge_ble_manager_post_event_with_payload(
            ZMK_USB_BRIDGE_EVENT_BOND_MISMATCH,
            ZMK_USB_BRIDGE_EVENT_REASON_BOND_AUTH_MISMATCH,
            status_code,
            conn_handle,
            ZMK_USB_BRIDGE_CAPABILITY_NONE
        );
    }

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
    const bool security_ready = g_security_ready;

    clear_connect_attempt_timeout();
    (void)zmk_usb_bridge_hog_client_reset();
    g_security_ready = false;

    if (g_active_conn != NULL) {
        bt_conn_unref(g_active_conn);
        g_active_conn = NULL;
    }

    if (g_active_conn_handle == conn_handle) {
        g_active_conn_handle = 0;
    }

    LOG_INF(
        "disconnected conn_handle=%u reason=%d status_code=%d hci_name=%s security_ready=%d",
        conn_handle,
        reason,
        (int)status_code,
        hci_err_name((uint8_t)status_code),
        security_ready
    );
    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_DISCONNECTED,
        reason,
        status_code,
        conn_handle,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}
