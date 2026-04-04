#include "zmk_usb_bridge/ble_reconnect.h"
#include "zmk_usb_bridge/ble_scan.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ble_reconn, LOG_LEVEL_INF);

enum {
    ZMK_USB_BRIDGE_FAST_ATTEMPT_COUNT = 4,
    ZMK_USB_BRIDGE_FRESH_OBSERVATION_SETTLE_MS = 750,
};

static const int32_t g_fast_delay_ms[ZMK_USB_BRIDGE_FAST_ATTEMPT_COUNT] = {0, 500, 1000, 2000};

typedef enum {
    ZMK_USB_BRIDGE_RECONNECT_MODE_IDLE = 0,
    ZMK_USB_BRIDGE_RECONNECT_MODE_FAST,
    ZMK_USB_BRIDGE_RECONNECT_MODE_BACKOFF,
} zmk_usb_bridge_reconnect_mode_t;

typedef struct {
    zmk_usb_bridge_reconnect_mode_t mode;
    bool peer_visible;
    bool attempt_armed;
    uint8_t fast_attempt_index;
    int32_t backoff_delay_ms;
    struct k_work_delayable attempt_work;
    struct k_work_delayable visibility_work;
} zmk_usb_bridge_reconnect_state_t;

static zmk_usb_bridge_reconnect_state_t g_reconnect;

static void attempt_timer_fired(struct k_work *work) {
    zmk_usb_bridge_status_t status;

    ARG_UNUSED(work);

    g_reconnect.attempt_armed = true;
    LOG_INF("attempt armed mode=%d", g_reconnect.mode);

    if (!g_reconnect.peer_visible) {
        return;
    }

    status = zmk_usb_bridge_ble_scan_trigger_known_device_attempt();
    if (status == ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_INF("attempt trigger dispatched from reconnect timer");
    }
}

static void visibility_timer_fired(struct k_work *work) {
    ARG_UNUSED(work);

    g_reconnect.peer_visible = false;
    g_reconnect.attempt_armed = false;
    (void)k_work_cancel_delayable(&g_reconnect.attempt_work);
    LOG_INF("peer invisible");
}

static void schedule_attempt_in(int32_t delay_ms) {
    if (delay_ms <= 0) {
        (void)k_work_cancel_delayable(&g_reconnect.attempt_work);
        g_reconnect.attempt_armed = true;
        LOG_INF("attempt armed immediately mode=%d", g_reconnect.mode);
        return;
    }

    g_reconnect.attempt_armed = false;
    k_work_reschedule(&g_reconnect.attempt_work, K_MSEC(delay_ms));
    LOG_INF("attempt scheduled in %d ms mode=%d", (int)delay_ms, g_reconnect.mode);
}

static void arm_visibility_timeout(void) {
    k_work_reschedule(&g_reconnect.visibility_work, K_SECONDS(2));
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_init(void) {
    g_reconnect = (zmk_usb_bridge_reconnect_state_t) {
        .mode = ZMK_USB_BRIDGE_RECONNECT_MODE_IDLE,
        .peer_visible = false,
        .attempt_armed = false,
        .fast_attempt_index = 0,
        .backoff_delay_ms = 5000,
    };
    k_work_init_delayable(&g_reconnect.attempt_work, attempt_timer_fired);
    k_work_init_delayable(&g_reconnect.visibility_work, visibility_timer_fired);
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_enter_fast_mode(void) {
    if (g_reconnect.mode == ZMK_USB_BRIDGE_RECONNECT_MODE_FAST) {
        LOG_INF(
            "keep fast reconnect mode attempt_index=%u peer_visible=%d armed=%d",
            g_reconnect.fast_attempt_index,
            g_reconnect.peer_visible,
            g_reconnect.attempt_armed
        );
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_FAST;
    g_reconnect.fast_attempt_index = 0;
    g_reconnect.backoff_delay_ms = 5000;
    schedule_attempt_in(0);
    LOG_INF("enter fast reconnect mode");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_enter_backoff_mode(void) {
    g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_BACKOFF;
    if (g_reconnect.backoff_delay_ms <= 0) {
        g_reconnect.backoff_delay_ms = 5000;
    }

    schedule_attempt_in(g_reconnect.backoff_delay_ms);
    LOG_INF("enter backoff reconnect mode delay_ms=%d", (int)g_reconnect.backoff_delay_ms);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_reset_fast_reconnect(void) {
    g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_FAST;
    g_reconnect.fast_attempt_index = 0;
    g_reconnect.backoff_delay_ms = 5000;
    g_reconnect.peer_visible = false;
    schedule_attempt_in(0);
    LOG_INF("reset_fast_reconnect");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_note_known_peer_seen(void) {
    const bool was_visible = g_reconnect.peer_visible;

    g_reconnect.peer_visible = true;
    arm_visibility_timeout();

    if (!was_visible) {
        g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_FAST;
        g_reconnect.fast_attempt_index = 0;
        g_reconnect.backoff_delay_ms = 5000;
        schedule_attempt_in(ZMK_USB_BRIDGE_FRESH_OBSERVATION_SETTLE_MS);
        LOG_INF(
            "fresh re-observation -> fast reconnect after settle_ms=%d",
            ZMK_USB_BRIDGE_FRESH_OBSERVATION_SETTLE_MS
        );
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_note_attempt_started(void) {
    g_reconnect.attempt_armed = false;
    (void)k_work_cancel_delayable(&g_reconnect.attempt_work);
    LOG_INF("attempt started mode=%d", g_reconnect.mode);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_note_failure(void) {
    if (g_reconnect.mode == ZMK_USB_BRIDGE_RECONNECT_MODE_IDLE) {
        g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_FAST;
    }

    if (g_reconnect.mode == ZMK_USB_BRIDGE_RECONNECT_MODE_FAST) {
        if (g_reconnect.fast_attempt_index + 1U < ZMK_USB_BRIDGE_FAST_ATTEMPT_COUNT) {
            g_reconnect.fast_attempt_index++;
            schedule_attempt_in(g_fast_delay_ms[g_reconnect.fast_attempt_index]);
            LOG_INF(
                "fast reconnect failure attempt_index=%u next_delay_ms=%d",
                g_reconnect.fast_attempt_index,
                (int)g_fast_delay_ms[g_reconnect.fast_attempt_index]
            );
            return ZMK_USB_BRIDGE_STATUS_OK;
        }

        g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_BACKOFF;
        g_reconnect.backoff_delay_ms = 5000;
        schedule_attempt_in(g_reconnect.backoff_delay_ms);
        LOG_INF("fast reconnect exhausted -> backoff");
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    if (g_reconnect.backoff_delay_ms <= 0) {
        g_reconnect.backoff_delay_ms = 5000;
    } else if (g_reconnect.backoff_delay_ms < 10000) {
        g_reconnect.backoff_delay_ms = 10000;
    }

    schedule_attempt_in(g_reconnect.backoff_delay_ms);
    LOG_INF("backoff reconnect failure next_delay_ms=%d", (int)g_reconnect.backoff_delay_ms);
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_ble_reconnect_note_connected(void) {
    g_reconnect.mode = ZMK_USB_BRIDGE_RECONNECT_MODE_IDLE;
    g_reconnect.peer_visible = true;
    g_reconnect.attempt_armed = false;
    g_reconnect.fast_attempt_index = 0;
    g_reconnect.backoff_delay_ms = 5000;
    (void)k_work_cancel_delayable(&g_reconnect.attempt_work);
    (void)k_work_cancel_delayable(&g_reconnect.visibility_work);
    LOG_INF("connected -> reconnect idle");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

bool zmk_usb_bridge_ble_reconnect_should_attempt_now(void) {
    if (!g_reconnect.peer_visible) {
        return false;
    }

    if (g_reconnect.mode == ZMK_USB_BRIDGE_RECONNECT_MODE_IDLE) {
        return true;
    }

    return g_reconnect.attempt_armed;
}

bool zmk_usb_bridge_ble_reconnect_is_in_backoff_mode(void) {
    return g_reconnect.mode == ZMK_USB_BRIDGE_RECONNECT_MODE_BACKOFF;
}
