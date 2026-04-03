#include "zmk_usb_bridge/ble_scan.h"

#include "zmk_usb_bridge/ble_connection.h"
#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/ble_reconnect.h"
#include "zmk_usb_bridge/pairing_filter.h"
#include "zmk_usb_bridge/ble_runtime.h"

#include <limits.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <string.h>

LOG_MODULE_REGISTER(zub_ble_scan, LOG_LEVEL_INF);

enum {
    ZMK_USB_BRIDGE_SCAN_CACHE_SIZE = 8,
    ZMK_USB_BRIDGE_SCAN_NAME_MAX = 32,
    ZMK_USB_BRIDGE_SCAN_CANDIDATE_STALE_MS = 3000,
};

typedef struct {
    bool in_use;
    bool reported;
    bt_addr_le_t addr;
    bool connectable;
    bool has_hid_service;
    bool has_keyboard_appearance;
    bool has_local_name;
    int64_t last_seen_ms;
    char local_name[ZMK_USB_BRIDGE_SCAN_NAME_MAX + 1];
} zmk_usb_bridge_scan_candidate_state_t;

static bool g_scan_callbacks_registered;
static bool g_scan_active;
static bool g_pairing_mode;
static zmk_usb_bridge_scan_candidate_state_t g_candidate_cache[ZMK_USB_BRIDGE_SCAN_CACHE_SIZE];

static bool appearance_is_keyboard_family(uint16_t appearance) {
    return appearance == BT_APPEARANCE_HID_KEYBOARD || appearance == BT_APPEARANCE_GENERIC_HID;
}

static void reset_candidate_cache(void) {
    memset(g_candidate_cache, 0, sizeof(g_candidate_cache));
}

static void add_bond_to_accept_list(const struct bt_bond_info *info, void *user_data) {
    int *err = user_data;

    if (info == NULL || err == NULL || *err != 0) {
        return;
    }

    *err = bt_le_filter_accept_list_add(&info->addr);
}

static void initialize_candidate_slot(
    zmk_usb_bridge_scan_candidate_state_t *slot,
    const bt_addr_le_t *addr,
    int64_t now_ms
) {
    slot->in_use = true;
    bt_addr_le_copy(&slot->addr, addr);
    slot->reported = false;
    slot->connectable = false;
    slot->has_hid_service = false;
    slot->has_keyboard_appearance = false;
    slot->has_local_name = false;
    slot->last_seen_ms = now_ms;
    memset(slot->local_name, 0, sizeof(slot->local_name));
}

static zmk_usb_bridge_scan_candidate_state_t *find_candidate_slot(const bt_addr_le_t *addr) {
    const int64_t now_ms = k_uptime_get();
    zmk_usb_bridge_scan_candidate_state_t *free_slot = NULL;
    zmk_usb_bridge_scan_candidate_state_t *stale_slot = NULL;
    zmk_usb_bridge_scan_candidate_state_t *oldest_slot = NULL;
    int64_t oldest_seen_ms = INT64_MAX;

    for (size_t i = 0; i < ARRAY_SIZE(g_candidate_cache); i++) {
        if (g_candidate_cache[i].in_use && bt_addr_le_eq(&g_candidate_cache[i].addr, addr)) {
            g_candidate_cache[i].last_seen_ms = now_ms;
            return &g_candidate_cache[i];
        }

        if (!g_candidate_cache[i].in_use && free_slot == NULL) {
            free_slot = &g_candidate_cache[i];
            continue;
        }

        if (g_candidate_cache[i].in_use &&
            (now_ms - g_candidate_cache[i].last_seen_ms) >= ZMK_USB_BRIDGE_SCAN_CANDIDATE_STALE_MS &&
            stale_slot == NULL) {
            stale_slot = &g_candidate_cache[i];
        }

        if (g_candidate_cache[i].in_use && g_candidate_cache[i].last_seen_ms < oldest_seen_ms) {
            oldest_seen_ms = g_candidate_cache[i].last_seen_ms;
            oldest_slot = &g_candidate_cache[i];
        }
    }

    if (free_slot != NULL) {
        initialize_candidate_slot(free_slot, addr, now_ms);
        return free_slot;
    }

    if (stale_slot != NULL) {
        LOG_INF("recycling stale scan candidate slot");
        initialize_candidate_slot(stale_slot, addr, now_ms);
        return stale_slot;
    }

    if (oldest_slot != NULL) {
        LOG_INF("recycling oldest scan candidate slot");
        initialize_candidate_slot(oldest_slot, addr, now_ms);
        return oldest_slot;
    }

    return NULL;
}

static bool parse_ad_element(struct bt_data *data, void *user_data) {
    zmk_usb_bridge_scan_candidate_state_t *candidate = user_data;

    if (candidate == NULL || data == NULL) {
        return false;
    }

    switch (data->type) {
    case BT_DATA_UUID16_SOME:
    case BT_DATA_UUID16_ALL:
        for (size_t offset = 0; offset + sizeof(uint16_t) <= data->data_len;
             offset += sizeof(uint16_t)) {
            if (sys_get_le16(&data->data[offset]) == BT_UUID_HIDS_VAL) {
                candidate->has_hid_service = true;
                break;
            }
        }
        break;
    case BT_DATA_GAP_APPEARANCE:
        if (data->data_len >= sizeof(uint16_t)) {
            candidate->has_keyboard_appearance =
                candidate->has_keyboard_appearance ||
                appearance_is_keyboard_family(sys_get_le16(data->data));
        }
        break;
    case BT_DATA_NAME_COMPLETE:
    case BT_DATA_NAME_SHORTENED: {
        const size_t copy_len = MIN((size_t)data->data_len, ZMK_USB_BRIDGE_SCAN_NAME_MAX);
        memcpy(candidate->local_name, data->data, copy_len);
        candidate->local_name[copy_len] = '\0';
        candidate->has_local_name = copy_len > 0U;
        break;
    }
    default:
        break;
    }

    return true;
}

static void maybe_log_pairing_candidate(zmk_usb_bridge_scan_candidate_state_t *candidate) {
    zmk_usb_bridge_pairing_candidate_t pairing_candidate;

    if (candidate == NULL || candidate->reported) {
        return;
    }

    pairing_candidate = (zmk_usb_bridge_pairing_candidate_t) {
        .connectable = candidate->connectable,
        .has_hid_service = candidate->has_hid_service,
        .has_keyboard_appearance = candidate->has_keyboard_appearance,
        .local_name = candidate->has_local_name ? candidate->local_name : NULL,
    };

    if (!candidate->connectable || !candidate->has_hid_service || !candidate->has_keyboard_appearance) {
        return;
    }

    if (!zmk_usb_bridge_pairing_filter_name_allowed(pairing_candidate.local_name)) {
        LOG_INF(
            "pairing candidate rejected by allowlist name=%s",
            candidate->has_local_name ? candidate->local_name : "<none>"
        );
        return;
    }

    if (!zmk_usb_bridge_pairing_filter_accept_unbonded_candidate(&pairing_candidate)) {
        return;
    }

    candidate->reported = true;
    LOG_INF(
        "pairing candidate matched name=%s connectable=%d hid=%d keyboard_app=%d",
        candidate->has_local_name ? candidate->local_name : "<none>",
        candidate->connectable,
        candidate->has_hid_service,
        candidate->has_keyboard_appearance
    );
}

static void maybe_start_connect(const bt_addr_le_t *addr, bool known_device) {
    zmk_usb_bridge_status_t status;
    zmk_usb_bridge_event_type_t event_type =
        known_device ? ZMK_USB_BRIDGE_EVENT_KNOWN_DEVICE_FOUND
                     : ZMK_USB_BRIDGE_EVENT_PAIRING_CANDIDATE_FOUND;

    status = zmk_usb_bridge_ble_scan_stop();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("scan stop before connect failed status=%d", status);
        return;
    }

    status = zmk_usb_bridge_ble_connection_connect_peer(addr);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("connect peer failed status=%d", status);
        return;
    }

    if (known_device) {
        (void)zmk_usb_bridge_ble_reconnect_note_attempt_started();
    }

    status = zmk_usb_bridge_ble_manager_post_simple_event(event_type);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("post candidate event failed status=%d", status);
    }
}

static void on_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf) {
    zmk_usb_bridge_scan_candidate_state_t *candidate;

    if (info == NULL || buf == NULL || info->addr == NULL) {
        return;
    }

    candidate = find_candidate_slot(info->addr);
    if (candidate == NULL) {
        LOG_WRN("scan candidate cache full");
        return;
    }

    candidate->connectable = candidate->connectable ||
        ((info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0U);
    bt_data_parse(buf, parse_ad_element, candidate);

    if (g_pairing_mode) {
        maybe_log_pairing_candidate(candidate);
        if (candidate->reported) {
            maybe_start_connect(info->addr, false);
        }
    } else if ((info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0U) {
        (void)zmk_usb_bridge_ble_reconnect_note_known_peer_seen();
        if (!zmk_usb_bridge_ble_reconnect_should_attempt_now()) {
            return;
        }
        maybe_start_connect(info->addr, true);
    }

    LOG_DBG(
        "scan recv type=%u props=0x%04x rssi=%d pairing=%d name=%s hid=%d keyapp=%d",
        info->adv_type,
        info->adv_props,
        info->rssi,
        g_pairing_mode,
        candidate->has_local_name ? candidate->local_name : "<none>",
        candidate->has_hid_service,
        candidate->has_keyboard_appearance
    );
}

static void on_scan_timeout(void) {
    LOG_INF("scan timeout");
    g_scan_active = false;
}

static struct bt_le_scan_cb g_scan_callbacks = {
    .recv = on_scan_recv,
    .timeout = on_scan_timeout,
};

static zmk_usb_bridge_status_t start_scan(const struct bt_le_scan_param *param, bool pairing_mode) {
    int err;

    if (!zmk_usb_bridge_ble_runtime_is_ready()) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (g_scan_active) {
        if (g_pairing_mode == pairing_mode) {
            return ZMK_USB_BRIDGE_STATUS_OK;
        }

        err = bt_le_scan_stop();
        if (err != 0) {
            LOG_ERR("bt_le_scan_stop before restart failed err=%d", err);
            return (zmk_usb_bridge_status_t)err;
        }
        g_scan_active = false;
    }

    reset_candidate_cache();

    err = bt_le_scan_start(param, NULL);
    if (err != 0) {
        LOG_ERR("bt_le_scan_start failed err=%d pairing=%d", err, pairing_mode);
        return (zmk_usb_bridge_status_t)err;
    }

    g_scan_active = true;
    g_pairing_mode = pairing_mode;
    LOG_INF("scan started pairing=%d", pairing_mode);
    return zmk_usb_bridge_ble_manager_post_simple_event(ZMK_USB_BRIDGE_EVENT_SCAN_STARTED);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_init(void) {
    if (!g_scan_callbacks_registered) {
        bt_le_scan_cb_register(&g_scan_callbacks);
        g_scan_callbacks_registered = true;
    }

    g_scan_active = false;
    g_pairing_mode = false;
    reset_candidate_cache();
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_start_pairing(void) {
    return start_scan(BT_LE_SCAN_ACTIVE, true);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_start_known_device(void) {
    struct bt_le_scan_param param = *BT_LE_SCAN_PASSIVE;
    int err;

    err = bt_le_filter_accept_list_clear();
    if (err != 0) {
        LOG_ERR("accept list clear failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    err = 0;
    bt_foreach_bond(BT_ID_DEFAULT, add_bond_to_accept_list, &err);
    if (err != 0) {
        LOG_ERR("accept list add from bond failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    param.options |= BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;
    return start_scan(&param, false);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_scan_stop(void) {
    int err;

    if (!g_scan_active) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    err = bt_le_scan_stop();
    if (err != 0) {
        LOG_ERR("bt_le_scan_stop failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    g_scan_active = false;
    if (!g_pairing_mode) {
        (void)bt_le_filter_accept_list_clear();
    }
    reset_candidate_cache();
    LOG_INF("stop");
    return ZMK_USB_BRIDGE_STATUS_OK;
}
