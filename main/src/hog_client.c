#include "zmk_usb_bridge/hog_client.h"

#include "zmk_usb_bridge/ble_connection.h"
#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/ble_reconnect.h"
#include "zmk_usb_bridge/bridge.h"

#include <errno.h>
#include <string.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(zub_hog, LOG_LEVEL_INF);

enum {
    ZMK_USB_BRIDGE_HOG_MAX_REPORTS = 8,
    ZMK_USB_BRIDGE_HOG_INPUT_MAX_LEN = 8,
    ZMK_USB_BRIDGE_HOG_INPUT_QUEUE_DEPTH = 32,
    ZMK_USB_BRIDGE_HOG_INPUT_TASK_STACK_SIZE = 1536,
    ZMK_USB_BRIDGE_HOG_INPUT_TASK_PRIORITY = 6,
    ZMK_USB_BRIDGE_HOG_KEYBOARD_REPORT_ID = 1,
    ZMK_USB_BRIDGE_HOG_CONSUMER_REPORT_ID = 2,
    ZMK_USB_BRIDGE_HOG_MOUSE_REPORT_ID = 3,
    ZMK_USB_BRIDGE_HOG_MOUSE_PAYLOAD_LEN = 5,
};

typedef struct {
    zmk_usb_bridge_report_role_t role;
    uint8_t payload[ZMK_USB_BRIDGE_HOG_INPUT_MAX_LEN];
    uint8_t payload_len;
} zmk_usb_bridge_hog_input_event_t;

typedef struct {
    bool in_use;
    uint8_t properties;
    uint16_t report_ref_handle;
    uint16_t end_handle;
    zmk_usb_bridge_report_binding_t binding;
    struct bt_gatt_subscribe_params subscribe_params;
} zmk_usb_bridge_hog_report_slot_t;

typedef struct {
    bool discovery_active;
    bool finalize_pending;
    uint16_t conn_handle;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    size_t report_count;
    size_t current_report_index;
    size_t pending_subscriptions;
    struct bt_conn *conn;
    struct bt_gatt_discover_params discover_params;
    struct bt_gatt_read_params read_params;
    zmk_usb_bridge_hog_report_slot_t reports[ZMK_USB_BRIDGE_HOG_MAX_REPORTS];
} zmk_usb_bridge_hog_context_t;

static zmk_usb_bridge_hog_profile_t profile;
static zmk_usb_bridge_hog_context_t g_ctx;
static bool g_input_thread_started;
static struct k_thread g_input_thread;

K_MSGQ_DEFINE(
    g_input_queue,
    sizeof(zmk_usb_bridge_hog_input_event_t),
    ZMK_USB_BRIDGE_HOG_INPUT_QUEUE_DEPTH,
    4
);
K_THREAD_STACK_DEFINE(g_input_task_stack, ZMK_USB_BRIDGE_HOG_INPUT_TASK_STACK_SIZE);

static zmk_usb_bridge_status_t start_service_discovery(void);
static zmk_usb_bridge_status_t start_report_discovery(void);
static zmk_usb_bridge_status_t start_descriptor_discovery(size_t index);
static zmk_usb_bridge_status_t start_report_ref_read(size_t index);
static zmk_usb_bridge_status_t start_subscriptions(void);

static bool conn_matches(const struct bt_conn *conn) {
    return g_ctx.conn != NULL && conn == g_ctx.conn;
}

static zmk_usb_bridge_hog_report_slot_t *current_report_slot(void) {
    if (g_ctx.current_report_index >= g_ctx.report_count) {
        return NULL;
    }

    return &g_ctx.reports[g_ctx.current_report_index];
}

static void set_profile_capability(zmk_usb_bridge_report_role_t role, bool enabled) {
    switch (role) {
    case ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT:
        profile.has_keyboard_input = enabled;
        break;
    case ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT:
        profile.has_consumer_input = enabled;
        break;
    case ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT:
        profile.has_mouse_input = enabled;
        break;
    case ZMK_USB_BRIDGE_ROLE_MOUSE_FEATURE:
        profile.has_mouse_feature = enabled;
        break;
    case ZMK_USB_BRIDGE_ROLE_LED_OUTPUT:
        profile.has_led_output = enabled;
        break;
    default:
        break;
    }
}

static zmk_usb_bridge_report_role_t role_from_report_ref(uint8_t report_id, uint8_t report_type) {
    if (report_type == ZMK_USB_BRIDGE_REPORT_TYPE_INPUT) {
        switch (report_id) {
        case ZMK_USB_BRIDGE_HOG_KEYBOARD_REPORT_ID:
            return ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT;
        case ZMK_USB_BRIDGE_HOG_CONSUMER_REPORT_ID:
            return ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT;
        case ZMK_USB_BRIDGE_HOG_MOUSE_REPORT_ID:
            return ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT;
        default:
            return ZMK_USB_BRIDGE_ROLE_UNKNOWN;
        }
    }

    if (report_type == ZMK_USB_BRIDGE_REPORT_TYPE_OUTPUT &&
        report_id == ZMK_USB_BRIDGE_HOG_KEYBOARD_REPORT_ID) {
        return ZMK_USB_BRIDGE_ROLE_LED_OUTPUT;
    }

    if (report_type == ZMK_USB_BRIDGE_REPORT_TYPE_FEATURE &&
        report_id == ZMK_USB_BRIDGE_HOG_MOUSE_REPORT_ID) {
        return ZMK_USB_BRIDGE_ROLE_MOUSE_FEATURE;
    }

    return ZMK_USB_BRIDGE_ROLE_UNKNOWN;
}

static zmk_usb_bridge_status_t bridge_queued_input(
    zmk_usb_bridge_report_role_t role,
    const uint8_t *payload,
    size_t payload_len
) {
    switch (role) {
    case ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT:
    case ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT:
        return zmk_usb_bridge_bridge_handle_input(role, payload, payload_len);
    case ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT:
        if (payload_len != ZMK_USB_BRIDGE_HOG_MOUSE_PAYLOAD_LEN) {
            return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
        }

        {
            const int8_t *mouse_payload = (const int8_t *)payload;
            const zmk_usb_bridge_mouse_body_t mouse = {
                .buttons = payload[0],
                .dx = mouse_payload[1],
                .dy = mouse_payload[2],
                .scroll_y = mouse_payload[3],
                .scroll_x = mouse_payload[4],
            };

            return zmk_usb_bridge_bridge_handle_input(role, &mouse, sizeof(mouse));
        }
    default:
        return ZMK_USB_BRIDGE_STATUS_OK;
    }
}

static void hog_input_task(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    zmk_usb_bridge_hog_input_event_t event;

    while (true) {
        if (k_msgq_get(&g_input_queue, &event, K_FOREVER) != 0) {
            continue;
        }

        const zmk_usb_bridge_status_t status = bridge_queued_input(
            event.role,
            event.payload,
            event.payload_len
        );
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            LOG_WRN(
                "bridge input failed role=%d payload_len=%u status=%d",
                event.role,
                event.payload_len,
                status
            );
        }
    }
}

static zmk_usb_bridge_hog_report_slot_t *find_slot_by_subscribe_params(
    struct bt_gatt_subscribe_params *params
) {
    if (params == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < g_ctx.report_count; i++) {
        if (&g_ctx.reports[i].subscribe_params == params) {
            return &g_ctx.reports[i];
        }
    }

    return NULL;
}

static zmk_usb_bridge_status_t advance_after_report_ref(void) {
    g_ctx.current_report_index++;

    if (g_ctx.current_report_index >= g_ctx.report_count) {
        return start_subscriptions();
    }

    return start_descriptor_discovery(g_ctx.current_report_index);
}

static void finalize_subscription_phase(void) {
    zmk_usb_bridge_status_t status;

    if (g_ctx.finalize_pending || g_ctx.pending_subscriptions != 0U) {
        return;
    }

    g_ctx.finalize_pending = true;

    if (!profile.has_keyboard_input) {
        status = zmk_usb_bridge_hog_client_fail_discovery(
            g_ctx.conn_handle,
            ZMK_USB_BRIDGE_EVENT_REASON_HID_SUBSCRIBE_FAILED,
            ZMK_USB_BRIDGE_STATUS_NOT_FOUND
        );
    } else {
        status = zmk_usb_bridge_hog_client_complete_discovery(g_ctx.conn_handle);
    }

    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("finalize discovery failed status=%d", status);
    }
}

static uint8_t on_report_notify(
    struct bt_conn *conn,
    struct bt_gatt_subscribe_params *params,
    const void *data,
    uint16_t length
) {
    zmk_usb_bridge_hog_input_event_t event;
    zmk_usb_bridge_hog_report_slot_t *slot = find_slot_by_subscribe_params(params);

    if (!conn_matches(conn) || slot == NULL) {
        return BT_GATT_ITER_STOP;
    }

    if (data == NULL) {
        slot->binding.subscribed = false;
        set_profile_capability(slot->binding.role, false);
        return BT_GATT_ITER_STOP;
    }

    if (length > sizeof(event.payload)) {
        LOG_WRN("input payload too large role=%d len=%u", slot->binding.role, length);
        return BT_GATT_ITER_CONTINUE;
    }

    event.role = slot->binding.role;
    event.payload_len = (uint8_t)length;
    memcpy(event.payload, data, length);

    if (k_msgq_put(&g_input_queue, &event, K_NO_WAIT) != 0) {
        LOG_WRN("drop input role=%d len=%u due queue full", event.role, event.payload_len);
    }

    return BT_GATT_ITER_CONTINUE;
}

static void on_report_subscribed(
    struct bt_conn *conn,
    uint8_t err,
    struct bt_gatt_subscribe_params *params
) {
    zmk_usb_bridge_hog_report_slot_t *slot = find_slot_by_subscribe_params(params);

    if (!conn_matches(conn) || slot == NULL) {
        return;
    }

    if (g_ctx.pending_subscriptions > 0U) {
        g_ctx.pending_subscriptions--;
    }

    if (err != 0U) {
        LOG_WRN(
            "subscribe failed role=%d report_id=%u err=%u",
            slot->binding.role,
            slot->binding.report_id,
            err
        );

        if (slot->binding.role == ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT) {
            (void)zmk_usb_bridge_hog_client_fail_discovery(
                g_ctx.conn_handle,
                ZMK_USB_BRIDGE_EVENT_REASON_HID_SUBSCRIBE_FAILED,
                err
            );
            return;
        }

        finalize_subscription_phase();
        return;
    }

    slot->binding.subscribed = true;
    set_profile_capability(slot->binding.role, true);
    LOG_INF(
        "subscribed role=%d report_id=%u value_handle=0x%04x",
        slot->binding.role,
        slot->binding.report_id,
        slot->binding.value_handle
    );
    finalize_subscription_phase();
}

static uint8_t on_report_ref_read(
    struct bt_conn *conn,
    uint8_t err,
    struct bt_gatt_read_params *params,
    const void *data,
    uint16_t length
) {
    zmk_usb_bridge_hog_report_slot_t *slot = current_report_slot();

    ARG_UNUSED(params);

    if (!conn_matches(conn) || slot == NULL) {
        return BT_GATT_ITER_STOP;
    }

    if (data == NULL) {
        return BT_GATT_ITER_STOP;
    }

    if (err != 0U || length < 2U) {
        LOG_WRN("report ref read failed err=%u len=%u", err, length);
        (void)advance_after_report_ref();
        return BT_GATT_ITER_STOP;
    }

    slot->binding.report_id = ((const uint8_t *)data)[0];
    slot->binding.report_type = (zmk_usb_bridge_report_type_t)((const uint8_t *)data)[1];
    slot->binding.role =
        role_from_report_ref(slot->binding.report_id, slot->binding.report_type);

    if (slot->binding.role == ZMK_USB_BRIDGE_ROLE_LED_OUTPUT ||
        slot->binding.role == ZMK_USB_BRIDGE_ROLE_MOUSE_FEATURE) {
        set_profile_capability(slot->binding.role, true);
    }

    LOG_INF(
        "report ref id=%u type=%u role=%d ccc=0x%04x",
        slot->binding.report_id,
        slot->binding.report_type,
        slot->binding.role,
        slot->binding.ccc_handle
    );

    (void)advance_after_report_ref();
    return BT_GATT_ITER_STOP;
}

static uint8_t on_report_descriptor_discovered(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    struct bt_gatt_discover_params *params
) {
    zmk_usb_bridge_hog_report_slot_t *slot = current_report_slot();

    ARG_UNUSED(params);

    if (!conn_matches(conn) || slot == NULL) {
        return BT_GATT_ITER_STOP;
    }

    if (attr == NULL) {
        if (slot->report_ref_handle == 0U) {
            LOG_DBG("skip report without report-ref value_handle=0x%04x", slot->binding.value_handle);
            (void)advance_after_report_ref();
            return BT_GATT_ITER_STOP;
        }

        if (start_report_ref_read(g_ctx.current_report_index) != ZMK_USB_BRIDGE_STATUS_OK) {
            (void)zmk_usb_bridge_hog_client_fail_discovery(
                g_ctx.conn_handle,
                ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
                ZMK_USB_BRIDGE_STATUS_INVALID_STATE
            );
        }
        return BT_GATT_ITER_STOP;
    }

    if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC)) {
        slot->binding.ccc_handle = attr->handle;
    } else if (!bt_uuid_cmp(attr->uuid, BT_UUID_HIDS_REPORT_REF)) {
        slot->report_ref_handle = attr->handle;
    }

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t on_report_characteristic_discovered(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    struct bt_gatt_discover_params *params
) {
    ARG_UNUSED(params);

    if (!conn_matches(conn)) {
        return BT_GATT_ITER_STOP;
    }

    if (attr == NULL) {
        if (g_ctx.report_count == 0U) {
            (void)zmk_usb_bridge_hog_client_fail_discovery(
                g_ctx.conn_handle,
                ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
                ZMK_USB_BRIDGE_STATUS_NOT_FOUND
            );
            return BT_GATT_ITER_STOP;
        }

        g_ctx.current_report_index = 0U;
        if (start_descriptor_discovery(0U) != ZMK_USB_BRIDGE_STATUS_OK) {
            (void)zmk_usb_bridge_hog_client_fail_discovery(
                g_ctx.conn_handle,
                ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
                ZMK_USB_BRIDGE_STATUS_INVALID_STATE
            );
        }
        return BT_GATT_ITER_STOP;
    }

    if (g_ctx.report_count >= ARRAY_SIZE(g_ctx.reports)) {
        LOG_WRN("too many HID reports, truncating");
        return BT_GATT_ITER_STOP;
    }

    {
        const struct bt_gatt_chrc *chrc = attr->user_data;
        zmk_usb_bridge_hog_report_slot_t *slot = &g_ctx.reports[g_ctx.report_count];

        if (g_ctx.report_count > 0U) {
            g_ctx.reports[g_ctx.report_count - 1U].end_handle = attr->handle - 1U;
        }

        memset(slot, 0, sizeof(*slot));
        slot->in_use = true;
        slot->properties = chrc->properties;
        slot->end_handle = g_ctx.service_end_handle;
        slot->binding.value_handle = chrc->value_handle;
        g_ctx.report_count++;
    }

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t on_hids_service_discovered(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    struct bt_gatt_discover_params *params
) {
    ARG_UNUSED(params);

    if (!conn_matches(conn)) {
        return BT_GATT_ITER_STOP;
    }

    if (attr == NULL) {
        (void)zmk_usb_bridge_hog_client_fail_discovery(
            g_ctx.conn_handle,
            ZMK_USB_BRIDGE_EVENT_REASON_HID_SERVICE_MISSING,
            ZMK_USB_BRIDGE_STATUS_NOT_FOUND
        );
        return BT_GATT_ITER_STOP;
    }

    {
        const struct bt_gatt_service_val *service = attr->user_data;
        g_ctx.service_start_handle = attr->handle;
        g_ctx.service_end_handle = service->end_handle;
    }

    if (start_report_discovery() != ZMK_USB_BRIDGE_STATUS_OK) {
        (void)zmk_usb_bridge_hog_client_fail_discovery(
            g_ctx.conn_handle,
            ZMK_USB_BRIDGE_EVENT_REASON_HID_SERVICE_MISSING,
            ZMK_USB_BRIDGE_STATUS_INVALID_STATE
        );
    }

    return BT_GATT_ITER_STOP;
}

static zmk_usb_bridge_status_t start_service_discovery(void) {
    int err;

    memset(&g_ctx.discover_params, 0, sizeof(g_ctx.discover_params));
    g_ctx.discover_params.uuid = BT_UUID_HIDS;
    g_ctx.discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    g_ctx.discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    g_ctx.discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    g_ctx.discover_params.func = on_hids_service_discovered;
    err = bt_gatt_discover(g_ctx.conn, &g_ctx.discover_params);
    if (err != 0) {
        LOG_WRN("start service discovery failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

static zmk_usb_bridge_status_t start_report_discovery(void) {
    int err;

    memset(&g_ctx.discover_params, 0, sizeof(g_ctx.discover_params));
    g_ctx.discover_params.uuid = BT_UUID_HIDS_REPORT;
    g_ctx.discover_params.start_handle = g_ctx.service_start_handle + 1U;
    g_ctx.discover_params.end_handle = g_ctx.service_end_handle;
    g_ctx.discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    g_ctx.discover_params.func = on_report_characteristic_discovered;
    err = bt_gatt_discover(g_ctx.conn, &g_ctx.discover_params);
    if (err != 0) {
        LOG_WRN("start report discovery failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

static zmk_usb_bridge_status_t start_descriptor_discovery(size_t index) {
    int err;
    zmk_usb_bridge_hog_report_slot_t *slot;

    if (index >= g_ctx.report_count) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    slot = &g_ctx.reports[index];
    if (slot->binding.value_handle >= slot->end_handle) {
        return advance_after_report_ref();
    }

    g_ctx.current_report_index = index;
    memset(&g_ctx.discover_params, 0, sizeof(g_ctx.discover_params));
    g_ctx.discover_params.uuid = NULL;
    g_ctx.discover_params.start_handle = slot->binding.value_handle + 1U;
    g_ctx.discover_params.end_handle = slot->end_handle;
    g_ctx.discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
    g_ctx.discover_params.func = on_report_descriptor_discovered;
    err = bt_gatt_discover(g_ctx.conn, &g_ctx.discover_params);
    if (err != 0) {
        LOG_WRN("start descriptor discovery failed err=%d index=%u", err, (unsigned int)index);
        return (zmk_usb_bridge_status_t)err;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

static zmk_usb_bridge_status_t start_report_ref_read(size_t index) {
    int err;
    zmk_usb_bridge_hog_report_slot_t *slot;

    if (index >= g_ctx.report_count) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    slot = &g_ctx.reports[index];
    if (slot->report_ref_handle == 0U) {
        return ZMK_USB_BRIDGE_STATUS_NOT_FOUND;
    }

    memset(&g_ctx.read_params, 0, sizeof(g_ctx.read_params));
    g_ctx.read_params.func = on_report_ref_read;
    g_ctx.read_params.handle_count = 1U;
    g_ctx.read_params.single.handle = slot->report_ref_handle;
    g_ctx.read_params.single.offset = 0U;
    err = bt_gatt_read(g_ctx.conn, &g_ctx.read_params);
    if (err != 0) {
        LOG_WRN("start report-ref read failed err=%d index=%u", err, (unsigned int)index);
        return (zmk_usb_bridge_status_t)err;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

static zmk_usb_bridge_status_t start_subscriptions(void) {
    bool saw_keyboard_candidate = false;

    g_ctx.pending_subscriptions = 0U;

    for (size_t i = 0; i < g_ctx.report_count; i++) {
        int err;
        zmk_usb_bridge_hog_report_slot_t *slot = &g_ctx.reports[i];

        if (!slot->in_use || slot->binding.role == ZMK_USB_BRIDGE_ROLE_UNKNOWN ||
            slot->binding.report_type != ZMK_USB_BRIDGE_REPORT_TYPE_INPUT) {
            continue;
        }

        if (!(slot->properties & BT_GATT_CHRC_NOTIFY) || slot->binding.ccc_handle == 0U) {
            if (slot->binding.role == ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT) {
                return zmk_usb_bridge_hog_client_fail_discovery(
                    g_ctx.conn_handle,
                    ZMK_USB_BRIDGE_EVENT_REASON_HID_SUBSCRIBE_FAILED,
                    ZMK_USB_BRIDGE_STATUS_NOT_FOUND
                );
            }

            LOG_WRN(
                "skip optional input role=%d notify=%d ccc=0x%04x",
                slot->binding.role,
                (slot->properties & BT_GATT_CHRC_NOTIFY) != 0U,
                slot->binding.ccc_handle
            );
            continue;
        }

        saw_keyboard_candidate = saw_keyboard_candidate ||
            slot->binding.role == ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT;

        memset(&slot->subscribe_params, 0, sizeof(slot->subscribe_params));
        slot->subscribe_params.notify = on_report_notify;
        slot->subscribe_params.subscribe = on_report_subscribed;
        slot->subscribe_params.value = BT_GATT_CCC_NOTIFY;
        slot->subscribe_params.value_handle = slot->binding.value_handle;
        slot->subscribe_params.ccc_handle = slot->binding.ccc_handle;
#if defined(CONFIG_BT_SMP)
        slot->subscribe_params.min_security = BT_SECURITY_L2;
#endif
        atomic_set_bit(slot->subscribe_params.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

        g_ctx.pending_subscriptions++;
        err = bt_gatt_subscribe(g_ctx.conn, &slot->subscribe_params);
        if (err == -EALREADY) {
            err = 0;
        }

        if (err != 0) {
            g_ctx.pending_subscriptions--;
            LOG_WRN(
                "bt_gatt_subscribe failed role=%d report_id=%u err=%d",
                slot->binding.role,
                slot->binding.report_id,
                err
            );

            if (slot->binding.role == ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT) {
                return zmk_usb_bridge_hog_client_fail_discovery(
                    g_ctx.conn_handle,
                    ZMK_USB_BRIDGE_EVENT_REASON_HID_SUBSCRIBE_FAILED,
                    err
                );
            }
        }
    }

    if (!saw_keyboard_candidate) {
        return zmk_usb_bridge_hog_client_fail_discovery(
            g_ctx.conn_handle,
            ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
            ZMK_USB_BRIDGE_STATUS_NOT_FOUND
        );
    }

    finalize_subscription_phase();
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_init(void) {
    memset(&profile, 0, sizeof(profile));
    memset(&g_ctx, 0, sizeof(g_ctx));

    if (!g_input_thread_started) {
        k_tid_t tid = k_thread_create(
            &g_input_thread,
            g_input_task_stack,
            K_THREAD_STACK_SIZEOF(g_input_task_stack),
            hog_input_task,
            NULL,
            NULL,
            NULL,
            ZMK_USB_BRIDGE_HOG_INPUT_TASK_PRIORITY,
            0,
            K_NO_WAIT
        );
        if (tid == NULL) {
            return ZMK_USB_BRIDGE_STATUS_NO_MEMORY;
        }

        k_thread_name_set(tid, "zub_hog_in");
        g_input_thread_started = true;
    }

    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_reset(void) {
    memset(&profile, 0, sizeof(profile));
    k_msgq_purge(&g_input_queue);

    if (g_ctx.conn != NULL) {
        bt_conn_unref(g_ctx.conn);
    }

    memset(&g_ctx, 0, sizeof(g_ctx));
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_start_discovery(
    struct bt_conn *conn,
    uint16_t conn_handle
) {
    zmk_usb_bridge_status_t status;

    if (conn == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    status = zmk_usb_bridge_hog_client_reset();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    g_ctx.conn = bt_conn_ref(conn);
    g_ctx.conn_handle = conn_handle;
    g_ctx.discovery_active = true;

    LOG_INF("start_discovery conn_handle=%u", conn_handle);
    return start_service_discovery();
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_set_profile(
    const zmk_usb_bridge_hog_profile_t *next_profile
) {
    if (next_profile == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    profile = *next_profile;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_complete_discovery(uint16_t conn_handle) {
    g_ctx.discovery_active = false;
    (void)zmk_usb_bridge_ble_reconnect_note_connected();

    if (!zmk_usb_bridge_hog_client_ready()) {
        return zmk_usb_bridge_ble_manager_post_event_with_payload(
            ZMK_USB_BRIDGE_EVENT_HID_FAILURE,
            ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
            ZMK_USB_BRIDGE_STATUS_NOT_FOUND,
            conn_handle,
            zmk_usb_bridge_hog_client_capability_flags()
        );
    }

    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_HID_READY,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        conn_handle,
        zmk_usb_bridge_hog_client_capability_flags()
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_hog_client_fail_discovery(
    uint16_t conn_handle,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code
) {
    g_ctx.discovery_active = false;
    (void)zmk_usb_bridge_ble_reconnect_note_failure();
    (void)zmk_usb_bridge_ble_connection_disconnect_active(BT_HCI_ERR_REMOTE_USER_TERM_CONN);

    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_HID_FAILURE,
        reason,
        status_code,
        conn_handle,
        zmk_usb_bridge_hog_client_capability_flags()
    );
}

bool zmk_usb_bridge_hog_client_ready(void) {
    return profile.has_keyboard_input;
}

uint16_t zmk_usb_bridge_hog_client_capability_flags(void) {
    uint16_t flags = ZMK_USB_BRIDGE_CAPABILITY_NONE;

    if (profile.has_keyboard_input) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_KEYBOARD_INPUT;
    }

    if (profile.has_consumer_input) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_CONSUMER_INPUT;
    }

    if (profile.has_mouse_input) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_MOUSE_INPUT;
    }

    if (profile.has_mouse_feature) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_MOUSE_FEATURE;
    }

    if (profile.has_led_output) {
        flags |= ZMK_USB_BRIDGE_CAPABILITY_LED_OUTPUT;
    }

    return flags;
}

const zmk_usb_bridge_hog_profile_t *zmk_usb_bridge_hog_client_profile(void) {
    return &profile;
}
