#include "zmk_usb_bridge/ble_manager.h"

#include "zmk_usb_bridge/ble_connection.h"
#include "zmk_usb_bridge/ble_reconnect.h"
#include "zmk_usb_bridge/ble_runtime.h"
#include "zmk_usb_bridge/ble_scan.h"
#include "zmk_usb_bridge/hog_client.h"
#include "zmk_usb_bridge/pairing_filter.h"
#include "zmk_usb_bridge/persist.h"
#include "zmk_usb_bridge/state_machine.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_mgr, LOG_LEVEL_INF);

enum {
    ZMK_USB_BRIDGE_EVENT_QUEUE_DEPTH = 16,
    ZMK_USB_BRIDGE_EVENT_TASK_STACK_SIZE = 2048,
    ZMK_USB_BRIDGE_EVENT_TASK_PRIORITY = 5,
};

K_MSGQ_DEFINE(g_event_queue, sizeof(zmk_usb_bridge_event_t), ZMK_USB_BRIDGE_EVENT_QUEUE_DEPTH, 4);
K_THREAD_STACK_DEFINE(g_event_task_stack, ZMK_USB_BRIDGE_EVENT_TASK_STACK_SIZE);

static struct k_thread g_event_thread;
static bool g_event_thread_started;

static zmk_usb_bridge_event_t make_event(
    zmk_usb_bridge_event_type_t type,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code,
    uint16_t conn_handle,
    uint16_t capability_flags
) {
    return (zmk_usb_bridge_event_t) {
        .type = type,
        .status_code = status_code,
        .conn_handle = conn_handle,
        .capability_flags = capability_flags,
        .reason = reason,
    };
}

static zmk_usb_bridge_status_t execute_command(zmk_usb_bridge_ble_command_t command) {
    switch (command) {
    case ZMK_USB_BRIDGE_BLE_COMMAND_NONE:
        return ZMK_USB_BRIDGE_STATUS_OK;
    case ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN:
        return zmk_usb_bridge_ble_scan_start_pairing();
    case ZMK_USB_BRIDGE_BLE_COMMAND_START_KNOWN_DEVICE_SCAN:
        return zmk_usb_bridge_ble_scan_start_known_device();
    case ZMK_USB_BRIDGE_BLE_COMMAND_ENTER_RECONNECT_FAST: {
        zmk_usb_bridge_status_t status = zmk_usb_bridge_ble_reconnect_enter_fast_mode();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }

        return zmk_usb_bridge_ble_scan_start_known_device();
    }
    case ZMK_USB_BRIDGE_BLE_COMMAND_ENTER_RECONNECT_BACKOFF: {
        zmk_usb_bridge_status_t status = zmk_usb_bridge_ble_reconnect_enter_backoff_mode();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }

        return zmk_usb_bridge_ble_scan_start_known_device();
    }
    case ZMK_USB_BRIDGE_BLE_COMMAND_STOP_SCAN:
        return zmk_usb_bridge_ble_scan_stop();
    case ZMK_USB_BRIDGE_BLE_COMMAND_ERASE_BONDS: {
        zmk_usb_bridge_status_t status = zmk_usb_bridge_ble_scan_stop();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }

        status = zmk_usb_bridge_ble_runtime_erase_bonds();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }

        return zmk_usb_bridge_persist_erase_all();
    }
    default:
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }
}

static zmk_usb_bridge_status_t init_submodules(void) {
    zmk_usb_bridge_status_t status = zmk_usb_bridge_pairing_filter_init();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = zmk_usb_bridge_ble_runtime_init();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = zmk_usb_bridge_ble_scan_init();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = zmk_usb_bridge_ble_connection_init();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = zmk_usb_bridge_ble_reconnect_init();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    return zmk_usb_bridge_hog_client_init();
}

static void zmk_usb_bridge_ble_manager_event_task(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    zmk_usb_bridge_event_t event;

    while (true) {
        if (k_msgq_get(&g_event_queue, &event, K_FOREVER) != 0) {
            continue;
        }

        const zmk_usb_bridge_status_t status = zmk_usb_bridge_state_machine_handle_event(&event);
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            LOG_WRN(
                "event dispatch failed type=%d reason=%d status_code=%d status=%d",
                event.type,
                event.reason,
                (int)event.status_code,
                status
            );
        }
    }
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_init(void) {
    LOG_INF("init");

    if (!g_event_thread_started) {
        k_tid_t tid = k_thread_create(
            &g_event_thread,
            g_event_task_stack,
            K_THREAD_STACK_SIZEOF(g_event_task_stack),
            zmk_usb_bridge_ble_manager_event_task,
            NULL,
            NULL,
            NULL,
            ZMK_USB_BRIDGE_EVENT_TASK_PRIORITY,
            0,
            K_NO_WAIT
        );
        if (tid == NULL) {
            return ZMK_USB_BRIDGE_STATUS_NO_MEMORY;
        }

        k_thread_name_set(tid, "zub_ble_evt");
        g_event_thread_started = true;
    }

    return init_submodules();
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_start_scan(void) {
    LOG_INF("start_scan");
    return zmk_usb_bridge_ble_scan_start_pairing();
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_reset_fast_reconnect(void) {
    zmk_usb_bridge_status_t status;

    status = zmk_usb_bridge_ble_reconnect_reset_fast_reconnect();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    LOG_INF("reset_fast_reconnect");
    return zmk_usb_bridge_ble_manager_post_simple_event(
        ZMK_USB_BRIDGE_EVENT_BUTTON_SHORT_PRESS
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_execute_command(
    zmk_usb_bridge_ble_command_t command
) {
    LOG_INF("execute command=%d", command);
    return execute_command(command);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_event(const zmk_usb_bridge_event_t *event) {
    if (event == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    if (!g_event_thread_started) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (k_msgq_put(&g_event_queue, event, K_NO_WAIT) != 0) {
        return ZMK_USB_BRIDGE_STATUS_QUEUE_FULL;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_simple_event(
    zmk_usb_bridge_event_type_t type
) {
    const zmk_usb_bridge_event_t event = make_event(
        type,
        ZMK_USB_BRIDGE_EVENT_REASON_NONE,
        0,
        0,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );

    return zmk_usb_bridge_ble_manager_post_event(&event);
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_manager_post_event_with_payload(
    zmk_usb_bridge_event_type_t type,
    zmk_usb_bridge_event_reason_t reason,
    int32_t status_code,
    uint16_t conn_handle,
    uint16_t capability_flags
) {
    const zmk_usb_bridge_event_t event = make_event(
        type,
        reason,
        status_code,
        conn_handle,
        capability_flags
    );

    return zmk_usb_bridge_ble_manager_post_event(&event);
}
