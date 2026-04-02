#include "zmk_usb_bridge/ble_manager.h"
#include "zmk_usb_bridge/persist.h"
#include "zmk_usb_bridge/recovery_ui.h"
#include "zmk_usb_bridge/state_machine.h"
#include "zmk_usb_bridge/usb_bridge.h"

#include <esp_err.h>
#include <esp_log.h>

static const char *TAG = "zub_app";

void app_main(void) {
    ESP_LOGI(TAG, "zmk-usb-bridge skeleton starting");

    ESP_ERROR_CHECK(zmk_usb_bridge_persist_init());
    ESP_ERROR_CHECK(zmk_usb_bridge_recovery_ui_init());
    ESP_ERROR_CHECK(zmk_usb_bridge_usb_bridge_init());
    ESP_ERROR_CHECK(zmk_usb_bridge_state_machine_init());
    ESP_ERROR_CHECK(zmk_usb_bridge_ble_manager_init());

    ESP_ERROR_CHECK(zmk_usb_bridge_state_machine_start());
}
