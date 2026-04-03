#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/persist.h"
#include "zmk_usb_bridge/recovery_ui.h"
#include "zmk_usb_bridge/state_machine.h"
#include "zmk_usb_bridge/usb_bridge.h"

#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_app, LOG_LEVEL_INF);

static bool check_status(const char *step, zmk_usb_bridge_status_t status) {
    if (status == ZMK_USB_BRIDGE_STATUS_OK) {
        return true;
    }

    LOG_ERR("%s failed status=%d", step, status);
    return false;
}

int main(void) {
    const zmk_usb_bridge_event_t ble_synced_event = {
        .type = ZMK_USB_BRIDGE_EVENT_BLE_SYNCED,
    };
    const zmk_usb_bridge_event_t persist_ready_event = {
        .type = ZMK_USB_BRIDGE_EVENT_PERSIST_READY_NO_BOND,
    };

    LOG_INF("zmk-usb-bridge Zephyr skeleton starting");

    if (!check_status("persist_init", zmk_usb_bridge_persist_init())) {
        return 0;
    }

    if (!check_status("recovery_ui_init", zmk_usb_bridge_recovery_ui_init())) {
        return 0;
    }

    if (!check_status("usb_bridge_init", zmk_usb_bridge_usb_bridge_init())) {
        return 0;
    }

    if (!check_status("state_machine_init", zmk_usb_bridge_state_machine_init())) {
        return 0;
    }

    if (!check_status("ble_manager_init", zmk_usb_bridge_ble_manager_init())) {
        return 0;
    }

    if (!check_status("state_machine_start", zmk_usb_bridge_state_machine_start())) {
        return 0;
    }

    if (!check_status("post_ble_synced", zmk_usb_bridge_ble_manager_post_event(&ble_synced_event))) {
        return 0;
    }

    if (!check_status("post_persist_ready", zmk_usb_bridge_ble_manager_post_event(&persist_ready_event))) {
        return 0;
    }

    LOG_INF("initial state=%d", zmk_usb_bridge_state_machine_current_state());
    return 0;
}
