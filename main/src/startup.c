#include "zmk_usb_bridge/startup.h"

#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/ble_runtime.h"
#include "zmk_usb_bridge/persist.h"
#include "zmk_usb_bridge/recovery_ui.h"
#include "zmk_usb_bridge/state_machine.h"
#include "zmk_usb_bridge/usb_bridge.h"

#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_startup, LOG_LEVEL_INF);

static bool check_status(const char *step, zmk_usb_bridge_status_t status) {
    if (status == ZMK_USB_BRIDGE_STATUS_OK) {
        return true;
    }

    LOG_ERR("%s failed status=%d", step, status);
    return false;
}

static zmk_usb_bridge_status_t post_startup_events(void) {
    zmk_usb_bridge_metadata_t metadata;
    zmk_usb_bridge_status_t status;
    bool metadata_fault = false;

    if (!zmk_usb_bridge_ble_runtime_is_ready()) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    status = zmk_usb_bridge_persist_load_metadata(&metadata);
    if (status == ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_INF("startup metadata loaded");
    } else if (status == ZMK_USB_BRIDGE_STATUS_NOT_FOUND) {
        LOG_INF("startup metadata not found");
    } else {
        LOG_WRN("startup metadata load failed status=%d; discarding", status);
        status = zmk_usb_bridge_persist_discard_metadata();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }
        metadata_fault = true;
    }

    status = zmk_usb_bridge_ble_manager_post_simple_event(
        ZMK_USB_BRIDGE_EVENT_BLE_SYNCED
    );
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = zmk_usb_bridge_ble_manager_post_simple_event(
        zmk_usb_bridge_ble_runtime_has_bond()
            ? ZMK_USB_BRIDGE_EVENT_PERSIST_READY_WITH_BOND
            : ZMK_USB_BRIDGE_EVENT_PERSIST_READY_NO_BOND
    );
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    if (!metadata_fault) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    return zmk_usb_bridge_ble_manager_post_event_with_payload(
        ZMK_USB_BRIDGE_EVENT_METADATA_FAULT,
        ZMK_USB_BRIDGE_EVENT_REASON_METADATA_CORRUPTED,
        0,
        0,
        ZMK_USB_BRIDGE_CAPABILITY_NONE
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_startup_run(void) {
    if (!check_status("persist_init", zmk_usb_bridge_persist_init())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (!check_status("recovery_ui_init", zmk_usb_bridge_recovery_ui_init())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (!check_status("usb_bridge_init", zmk_usb_bridge_usb_bridge_init())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (!check_status("state_machine_init", zmk_usb_bridge_state_machine_init())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (!check_status("ble_manager_init", zmk_usb_bridge_ble_manager_init())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (!check_status("state_machine_start", zmk_usb_bridge_state_machine_start())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    if (!check_status("post_startup_events", post_startup_events())) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    LOG_INF("initial state=%d", zmk_usb_bridge_state_machine_current_state());
    return ZMK_USB_BRIDGE_STATUS_OK;
}
