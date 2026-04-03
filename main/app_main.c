#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/startup.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_app, LOG_LEVEL_INF);

int main(void) {
    LOG_INF("zmk-usb-bridge Zephyr skeleton starting");
    if (zmk_usb_bridge_startup_run() != ZMK_USB_BRIDGE_STATUS_OK) {
        return 0;
    }
    return 0;
}
