#include "zmk_usb_bridge/ble_runtime.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_rt, LOG_LEVEL_INF);

static bool g_ready;
static size_t g_bond_count;
static bool g_auth_info_registered;

static void count_bond(const struct bt_bond_info *info, void *user_data) {
    size_t *count = user_data;

    ARG_UNUSED(info);

    if (count != NULL) {
        (*count)++;
    }
}

static void refresh_bond_inventory(void) {
    g_bond_count = 0;
    bt_foreach_bond(BT_ID_DEFAULT, count_bond, &g_bond_count);
}

static void on_pairing_complete(struct bt_conn *conn, bool bonded) {
    ARG_UNUSED(conn);

    if (!bonded) {
        return;
    }

    refresh_bond_inventory();
    LOG_INF("pairing complete; bonds=%u", (unsigned int)g_bond_count);
}

static void on_bond_deleted(uint8_t id, const bt_addr_le_t *peer) {
    ARG_UNUSED(id);
    ARG_UNUSED(peer);

    refresh_bond_inventory();
    LOG_INF("bond deleted; bonds=%u", (unsigned int)g_bond_count);
}

static struct bt_conn_auth_info_cb g_auth_info_cb = {
    .pairing_complete = on_pairing_complete,
    .bond_deleted = on_bond_deleted,
};

zmk_usb_bridge_status_t zmk_usb_bridge_ble_runtime_init(void) {
    int err;

    if (g_ready) {
        refresh_bond_inventory();
        LOG_INF("init already ready bonds=%u", (unsigned int)g_bond_count);
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    err = bt_enable(NULL);
    if (err != 0) {
        LOG_ERR("bt_enable failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    err = settings_load_subtree("bt");
    if (err != 0) {
        LOG_ERR("settings_load_subtree(bt) failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    if (!g_auth_info_registered) {
        err = bt_conn_auth_info_cb_register(&g_auth_info_cb);
        if (err != 0) {
            LOG_ERR("bt_conn_auth_info_cb_register failed err=%d", err);
            return (zmk_usb_bridge_status_t)err;
        }
        g_auth_info_registered = true;
    }

    g_ready = true;
    refresh_bond_inventory();
    LOG_INF("init ready bonds=%u", (unsigned int)g_bond_count);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_runtime_erase_bonds(void) {
    int err;

    if (!g_ready) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    err = bt_unpair(BT_ID_DEFAULT, NULL);
    if (err != 0) {
        LOG_ERR("bt_unpair failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    refresh_bond_inventory();
    LOG_INF("erase bonds complete remaining=%u", (unsigned int)g_bond_count);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

bool zmk_usb_bridge_ble_runtime_is_ready(void) {
    return g_ready;
}

bool zmk_usb_bridge_ble_runtime_has_bond(void) {
    return g_bond_count > 0U;
}

size_t zmk_usb_bridge_ble_runtime_bond_count(void) {
    return g_bond_count;
}
