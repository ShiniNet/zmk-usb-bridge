#include "zmk_usb_bridge/ble_manager.h"

#include "zmk_usb_bridge/hog_client.h"
#include "zmk_usb_bridge/pairing_filter.h"
#include "zmk_usb_bridge/state_machine.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

static const char *TAG = "zub_ble_mgr";
static QueueHandle_t g_event_queue;
static TaskHandle_t g_event_task;

enum {
    ZMK_USB_BRIDGE_EVENT_QUEUE_DEPTH = 16,
    ZMK_USB_BRIDGE_EVENT_TASK_STACK_WORDS = 4096,
    ZMK_USB_BRIDGE_EVENT_TASK_PRIORITY = 5,
};

static void zmk_usb_bridge_ble_manager_event_task(void *arg) {
    (void)arg;

    zmk_usb_bridge_event_t event;
    while (true) {
        if (xQueueReceive(g_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const esp_err_t err = zmk_usb_bridge_state_machine_handle_event(&event);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "event dispatch failed type=%d err=%s", event.type, esp_err_to_name(err));
        }
    }
}

esp_err_t zmk_usb_bridge_ble_manager_init(void) {
    ESP_LOGI(TAG, "init");

    if (g_event_queue == NULL) {
        g_event_queue = xQueueCreate(ZMK_USB_BRIDGE_EVENT_QUEUE_DEPTH, sizeof(zmk_usb_bridge_event_t));
        if (g_event_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (g_event_task == NULL) {
        const BaseType_t task_created = xTaskCreate(
            zmk_usb_bridge_ble_manager_event_task,
            "zub_ble_evt",
            ZMK_USB_BRIDGE_EVENT_TASK_STACK_WORDS,
            NULL,
            ZMK_USB_BRIDGE_EVENT_TASK_PRIORITY,
            &g_event_task
        );
        if (task_created != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_ERROR_CHECK(zmk_usb_bridge_pairing_filter_init());
    return zmk_usb_bridge_hog_client_init();
}

esp_err_t zmk_usb_bridge_ble_manager_start_scan(void) {
    ESP_LOGI(TAG, "start_scan");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_ble_manager_reset_fast_reconnect(void) {
    ESP_LOGI(TAG, "reset_fast_reconnect");
    const zmk_usb_bridge_event_t event = {
        .type = ZMK_USB_BRIDGE_EVENT_BUTTON_SHORT_PRESS,
    };
    return zmk_usb_bridge_ble_manager_post_event(&event);
}

esp_err_t zmk_usb_bridge_ble_manager_on_gap_event(const struct ble_gap_event *event) {
    (void)event;
    ESP_LOGD(TAG, "gap_event");
    return ESP_OK;
}

esp_err_t zmk_usb_bridge_ble_manager_post_event(const zmk_usb_bridge_event_t *event) {
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(g_event_queue, event, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
