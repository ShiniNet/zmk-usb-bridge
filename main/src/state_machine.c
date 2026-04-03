#include "zmk_usb_bridge/state_machine.h"

#include "zmk_usb_bridge/ble_command.h"
#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/ble_runtime.h"
#include "zmk_usb_bridge/bridge.h"
#include "zmk_usb_bridge/recovery_ui.h"

#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_sm, LOG_LEVEL_INF);

static zmk_usb_bridge_state_t g_state = ZMK_USB_BRIDGE_STATE_BOOT;
static bool g_usb_ready;
static bool g_ble_synced;
static bool g_persist_ready;
static bool g_has_bond;

static zmk_usb_bridge_status_t set_state(zmk_usb_bridge_state_t next_state) {
    g_state = next_state;
    LOG_INF("state=%d", next_state);
    return zmk_usb_bridge_recovery_ui_set_state(next_state);
}

static zmk_usb_bridge_status_t dispatch_ble_command(zmk_usb_bridge_ble_command_t command) {
    if (command == ZMK_USB_BRIDGE_BLE_COMMAND_NONE) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    return zmk_usb_bridge_ble_manager_execute_command(command);
}

static zmk_usb_bridge_status_t transition_to(
    zmk_usb_bridge_state_t next_state,
    zmk_usb_bridge_ble_command_t command
) {
    zmk_usb_bridge_status_t status = dispatch_ble_command(command);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("command=%d failed before state=%d status=%d", command, next_state, status);
        return status;
    }

    return set_state(next_state);
}

static zmk_usb_bridge_status_t transition_to_state_only(zmk_usb_bridge_state_t next_state) {
    return set_state(next_state);
}

static zmk_usb_bridge_status_t transition_to_and_post(
    zmk_usb_bridge_state_t next_state,
    zmk_usb_bridge_ble_command_t command,
    zmk_usb_bridge_event_type_t post_event_type
) {
    zmk_usb_bridge_status_t status = transition_to(next_state, command);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    if (post_event_type == ZMK_USB_BRIDGE_EVENT_NONE) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    return zmk_usb_bridge_ble_manager_post_simple_event(post_event_type);
}

static zmk_usb_bridge_status_t advance_startup_gate(void) {
    if (!g_usb_ready) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    if (g_state == ZMK_USB_BRIDGE_STATE_BOOT) {
        zmk_usb_bridge_status_t status = set_state(ZMK_USB_BRIDGE_STATE_USB_READY);
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }
    }

    if (!g_ble_synced || !g_persist_ready) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    if (g_state != ZMK_USB_BRIDGE_STATE_USB_READY) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    return transition_to(
        g_has_bond ? ZMK_USB_BRIDGE_STATE_SCANNING_KNOWN_DEVICE : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
        g_has_bond ? ZMK_USB_BRIDGE_BLE_COMMAND_START_KNOWN_DEVICE_SCAN
                   : ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN
    );
}

zmk_usb_bridge_status_t zmk_usb_bridge_state_machine_init(void) {
    zmk_usb_bridge_status_t status = zmk_usb_bridge_bridge_init();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    g_usb_ready = false;
    g_ble_synced = false;
    g_persist_ready = false;
    g_has_bond = false;
    return set_state(ZMK_USB_BRIDGE_STATE_BOOT);
}

zmk_usb_bridge_status_t zmk_usb_bridge_state_machine_start(void) {
    const zmk_usb_bridge_event_t event = {
        .type = ZMK_USB_BRIDGE_EVENT_USB_READY,
    };

    return zmk_usb_bridge_state_machine_handle_event(&event);
}

zmk_usb_bridge_status_t zmk_usb_bridge_state_machine_handle_event(const zmk_usb_bridge_event_t *event) {
    if (event == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    switch (event->type) {
    case ZMK_USB_BRIDGE_EVENT_USB_READY:
        g_usb_ready = true;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_BLE_SYNCED:
        g_ble_synced = true;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_PERSIST_READY_WITH_BOND:
        g_persist_ready = true;
        g_has_bond = true;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_PERSIST_READY_NO_BOND:
        g_persist_ready = true;
        g_has_bond = false;
        return advance_startup_gate();
    case ZMK_USB_BRIDGE_EVENT_KNOWN_DEVICE_FOUND:
    case ZMK_USB_BRIDGE_EVENT_PAIRING_CANDIDATE_FOUND:
    case ZMK_USB_BRIDGE_EVENT_CONNECT_SUCCESS:
        return transition_to_state_only(ZMK_USB_BRIDGE_STATE_CONNECTING);
    case ZMK_USB_BRIDGE_EVENT_CONNECT_FAILURE:
    case ZMK_USB_BRIDGE_EVENT_HID_FAILURE:
        return transition_to(
            g_has_bond ? ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
            g_has_bond ? ZMK_USB_BRIDGE_BLE_COMMAND_ENTER_RECONNECT_FAST
                       : ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN
        );
    case ZMK_USB_BRIDGE_EVENT_HID_READY:
        g_has_bond = g_has_bond || zmk_usb_bridge_ble_runtime_has_bond();
        return transition_to_state_only(ZMK_USB_BRIDGE_STATE_CONNECTED);
    case ZMK_USB_BRIDGE_EVENT_DISCONNECTED: {
        zmk_usb_bridge_status_t status = zmk_usb_bridge_bridge_release_all();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }
        return transition_to(
            g_has_bond ? ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
            g_has_bond ? ZMK_USB_BRIDGE_BLE_COMMAND_ENTER_RECONNECT_FAST
                       : ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN
        );
    }
    case ZMK_USB_BRIDGE_EVENT_HOST_RESET: {
        zmk_usb_bridge_status_t status;
        g_ble_synced = false;
        status = zmk_usb_bridge_bridge_release_all();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }
        return transition_to(ZMK_USB_BRIDGE_STATE_USB_READY, ZMK_USB_BRIDGE_BLE_COMMAND_STOP_SCAN);
    }
    case ZMK_USB_BRIDGE_EVENT_BOND_MISMATCH: {
        zmk_usb_bridge_status_t status = zmk_usb_bridge_bridge_release_all();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }
        return transition_to(
            ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED,
            ZMK_USB_BRIDGE_BLE_COMMAND_STOP_SCAN
        );
    }
    case ZMK_USB_BRIDGE_EVENT_METADATA_FAULT:
        return transition_to(
            g_has_bond ? ZMK_USB_BRIDGE_STATE_SCANNING_KNOWN_DEVICE : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
            g_has_bond ? ZMK_USB_BRIDGE_BLE_COMMAND_START_KNOWN_DEVICE_SCAN
                       : ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN
        );
    case ZMK_USB_BRIDGE_EVENT_BUTTON_SHORT_PRESS:
        if (g_state == ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED) {
            return ZMK_USB_BRIDGE_STATUS_OK;
        }
        return transition_to(
            g_has_bond ? ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST : ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
            g_has_bond ? ZMK_USB_BRIDGE_BLE_COMMAND_ENTER_RECONNECT_FAST
                       : ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN
        );
    case ZMK_USB_BRIDGE_EVENT_BUTTON_LONG_PRESS: {
        zmk_usb_bridge_status_t status = zmk_usb_bridge_bridge_release_all();
        if (status != ZMK_USB_BRIDGE_STATUS_OK) {
            return status;
        }
        return transition_to_and_post(
            ZMK_USB_BRIDGE_STATE_BOND_ERASING,
            ZMK_USB_BRIDGE_BLE_COMMAND_ERASE_BONDS,
            ZMK_USB_BRIDGE_EVENT_BOND_ERASE_COMPLETE
        );
    }
    case ZMK_USB_BRIDGE_EVENT_BOND_ERASE_COMPLETE:
        g_has_bond = false;
        g_persist_ready = true;
        return transition_to(
            ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
            ZMK_USB_BRIDGE_BLE_COMMAND_START_PAIRING_SCAN
        );
    default:
        return ZMK_USB_BRIDGE_STATUS_OK;
    }
}

zmk_usb_bridge_state_t zmk_usb_bridge_state_machine_current_state(void) {
    return g_state;
}
