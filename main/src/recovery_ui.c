#include "zmk_usb_bridge/recovery_ui.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ui, LOG_LEVEL_INF);

#define ZMK_USB_BRIDGE_LED_RED_NODE DT_ALIAS(led0)
#define ZMK_USB_BRIDGE_LED_BLUE_NODE DT_ALIAS(led1)
#define ZMK_USB_BRIDGE_LED_GREEN_NODE DT_ALIAS(led2)

typedef struct {
    bool red;
    bool green;
    bool blue;
} zmk_usb_bridge_led_color_t;

typedef struct {
    bool blinking;
    uint32_t on_ms;
    uint32_t off_ms;
    zmk_usb_bridge_led_color_t color;
} zmk_usb_bridge_led_pattern_t;

static const struct gpio_dt_spec g_led_red = GPIO_DT_SPEC_GET(ZMK_USB_BRIDGE_LED_RED_NODE, gpios);
static const struct gpio_dt_spec g_led_blue =
    GPIO_DT_SPEC_GET(ZMK_USB_BRIDGE_LED_BLUE_NODE, gpios);
static const struct gpio_dt_spec g_led_green =
    GPIO_DT_SPEC_GET(ZMK_USB_BRIDGE_LED_GREEN_NODE, gpios);

static zmk_usb_bridge_state_t g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;
static bool g_blink_phase_on;
static struct k_work_delayable g_led_work;

static void set_leds(const zmk_usb_bridge_led_color_t *color) {
    if (color == NULL) {
        return;
    }

    (void)gpio_pin_set_dt(&g_led_red, color->red ? 1 : 0);
    (void)gpio_pin_set_dt(&g_led_green, color->green ? 1 : 0);
    (void)gpio_pin_set_dt(&g_led_blue, color->blue ? 1 : 0);
}

static zmk_usb_bridge_led_pattern_t pattern_for_state(zmk_usb_bridge_state_t state) {
    switch (state) {
    case ZMK_USB_BRIDGE_STATE_PAIRING_SCAN:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = true,
            .on_ms = 500,
            .off_ms = 500,
            .color = {.blue = true},
        };
    case ZMK_USB_BRIDGE_STATE_SCANNING_KNOWN_DEVICE:
    case ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST:
    case ZMK_USB_BRIDGE_STATE_RECONNECTING_BACKOFF:
    case ZMK_USB_BRIDGE_STATE_CONNECTING:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = true,
            .on_ms = 500,
            .off_ms = 500,
            .color = {.red = true, .green = true},
        };
    case ZMK_USB_BRIDGE_STATE_CONNECTED:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = false,
            .color = {.green = true},
        };
    case ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = true,
            .on_ms = 900,
            .off_ms = 900,
            .color = {.red = true},
        };
    case ZMK_USB_BRIDGE_STATE_BOND_ERASING:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = true,
            .on_ms = 150,
            .off_ms = 150,
            .color = {.red = true},
        };
    case ZMK_USB_BRIDGE_STATE_FATAL_ERROR:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = false,
            .color = {.red = true},
        };
    case ZMK_USB_BRIDGE_STATE_BOOT:
    case ZMK_USB_BRIDGE_STATE_USB_READY:
    default:
        return (zmk_usb_bridge_led_pattern_t) {
            .blinking = false,
            .color = {0},
        };
    }
}

static void led_work_handler(struct k_work *work) {
    const zmk_usb_bridge_led_pattern_t pattern = pattern_for_state(g_ui_state);

    ARG_UNUSED(work);

    if (!pattern.blinking) {
        g_blink_phase_on = true;
        set_leds(&pattern.color);
        return;
    }

    if (g_blink_phase_on) {
        set_leds(&(zmk_usb_bridge_led_color_t){0});
        g_blink_phase_on = false;
        k_work_reschedule(&g_led_work, K_MSEC(pattern.off_ms));
        return;
    }

    set_leds(&pattern.color);
    g_blink_phase_on = true;
    k_work_reschedule(&g_led_work, K_MSEC(pattern.on_ms));
}

static zmk_usb_bridge_status_t configure_led(const struct gpio_dt_spec *spec, const char *label) {
    int err;

    if (spec == NULL || !device_is_ready(spec->port)) {
        LOG_ERR("%s not ready", label);
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    err = gpio_pin_configure_dt(spec, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
        LOG_ERR("%s configure failed err=%d", label, err);
        return (zmk_usb_bridge_status_t)err;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_init(void) {
    zmk_usb_bridge_status_t status;

    status = configure_led(&g_led_red, "red led");
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = configure_led(&g_led_green, "green led");
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = configure_led(&g_led_blue, "blue led");
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    k_work_init_delayable(&g_led_work, led_work_handler);
    g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;
    g_blink_phase_on = true;
    set_leds(&(zmk_usb_bridge_led_color_t){0});
    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_set_state(zmk_usb_bridge_state_t state) {
    g_ui_state = state;
    g_blink_phase_on = false;
    (void)k_work_cancel_delayable(&g_led_work);
    LOG_INF("ui_state=%d", state);
    led_work_handler(&g_led_work.work);
    return ZMK_USB_BRIDGE_STATUS_OK;
}
