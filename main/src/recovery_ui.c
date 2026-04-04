#include "zmk_usb_bridge/recovery_ui.h"
#include "zmk_usb_bridge/ble_manager.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zub_ui, LOG_LEVEL_INF);

#define ZMK_USB_BRIDGE_LED_RED_NODE DT_ALIAS(led0)
/*
 * On the Seeed XIAO BLE hardware revision used for bring-up, the physical
 * blue/green outputs appear swapped relative to the board DTS labels.
 * Map the user-facing UI colors to the observed physical LEDs.
 */
#define ZMK_USB_BRIDGE_LED_BLUE_NODE DT_ALIAS(led2)
#define ZMK_USB_BRIDGE_LED_GREEN_NODE DT_ALIAS(led1)
#define ZMK_USB_BRIDGE_RECOVERY_BUTTON_NODE DT_ALIAS(sw0)

#define ZMK_USB_BRIDGE_HAS_RECOVERY_BUTTON \
    DT_NODE_HAS_STATUS(ZMK_USB_BRIDGE_RECOVERY_BUTTON_NODE, okay)

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

#if ZMK_USB_BRIDGE_HAS_RECOVERY_BUTTON
static const struct gpio_dt_spec g_recovery_button =
    GPIO_DT_SPEC_GET(ZMK_USB_BRIDGE_RECOVERY_BUTTON_NODE, gpios);
static struct gpio_callback g_recovery_button_callback;
static struct k_work_delayable g_button_debounce_work;
static struct k_work_delayable g_button_hold_work;
#endif

static zmk_usb_bridge_state_t g_ui_state = ZMK_USB_BRIDGE_STATE_BOOT;
static bool g_blink_phase_on;
static struct k_work_delayable g_led_work;
static bool g_input_events_enabled;

#if ZMK_USB_BRIDGE_HAS_RECOVERY_BUTTON
static bool g_button_stable_pressed;
static bool g_button_long_press_fired;
#endif

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

#if ZMK_USB_BRIDGE_HAS_RECOVERY_BUTTON
static bool recovery_button_is_pressed(void) {
    return gpio_pin_get_dt(&g_recovery_button) > 0;
}

static void dispatch_short_press(void) {
    zmk_usb_bridge_status_t status;

    if (!g_input_events_enabled) {
        return;
    }

    status = zmk_usb_bridge_ble_manager_reset_fast_reconnect();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("button short press dispatch failed status=%d", status);
        return;
    }

    LOG_INF("button short press");
}

static void dispatch_long_press(void) {
    zmk_usb_bridge_status_t status;

    if (!g_input_events_enabled) {
        return;
    }

    status = zmk_usb_bridge_ble_manager_post_simple_event(
        ZMK_USB_BRIDGE_EVENT_BUTTON_LONG_PRESS
    );
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("button long press dispatch failed status=%d", status);
        return;
    }

    LOG_INF("button long press");
}

static void button_hold_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!g_button_stable_pressed || g_button_long_press_fired || !recovery_button_is_pressed()) {
        return;
    }

    g_button_long_press_fired = true;
    dispatch_long_press();
}

static void button_debounce_work_handler(struct k_work *work) {
    const bool pressed = recovery_button_is_pressed();

    ARG_UNUSED(work);

    if (pressed == g_button_stable_pressed) {
        return;
    }

    g_button_stable_pressed = pressed;
    if (pressed) {
        g_button_long_press_fired = false;
        k_work_reschedule(
            &g_button_hold_work,
            K_MSEC(CONFIG_ZMK_USB_BRIDGE_RECOVERY_BUTTON_LONG_PRESS_MS)
        );
        return;
    }

    (void)k_work_cancel_delayable(&g_button_hold_work);
    if (!g_button_long_press_fired) {
        dispatch_short_press();
    }
}

static void recovery_button_interrupt_handler(
    const struct device *port,
    struct gpio_callback *cb,
    uint32_t pins
) {
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_reschedule(
        &g_button_debounce_work,
        K_MSEC(CONFIG_ZMK_USB_BRIDGE_RECOVERY_BUTTON_DEBOUNCE_MS)
    );
}

static zmk_usb_bridge_status_t configure_recovery_button(void) {
    int err;

    if (!device_is_ready(g_recovery_button.port)) {
        LOG_ERR("recovery button not ready");
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    err = gpio_pin_configure_dt(&g_recovery_button, GPIO_INPUT);
    if (err != 0) {
        LOG_ERR("recovery button configure failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    err = gpio_pin_interrupt_configure_dt(&g_recovery_button, GPIO_INT_EDGE_BOTH);
    if (err != 0) {
        LOG_ERR("recovery button interrupt configure failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    gpio_init_callback(
        &g_recovery_button_callback,
        recovery_button_interrupt_handler,
        BIT(g_recovery_button.pin)
    );

    err = gpio_add_callback(g_recovery_button.port, &g_recovery_button_callback);
    if (err != 0) {
        LOG_ERR("recovery button add callback failed err=%d", err);
        return (zmk_usb_bridge_status_t)err;
    }

    k_work_init_delayable(&g_button_debounce_work, button_debounce_work_handler);
    k_work_init_delayable(&g_button_hold_work, button_hold_work_handler);
    g_button_stable_pressed = recovery_button_is_pressed();
    g_button_long_press_fired = false;
    LOG_INF(
        "recovery button ready pin=%u long_press_ms=%d debounce_ms=%d",
        g_recovery_button.pin,
        CONFIG_ZMK_USB_BRIDGE_RECOVERY_BUTTON_LONG_PRESS_MS,
        CONFIG_ZMK_USB_BRIDGE_RECOVERY_BUTTON_DEBOUNCE_MS
    );
    return ZMK_USB_BRIDGE_STATUS_OK;
}
#endif

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
    g_input_events_enabled = false;
    set_leds(&(zmk_usb_bridge_led_color_t){0});

#if ZMK_USB_BRIDGE_HAS_RECOVERY_BUTTON
    status = configure_recovery_button();
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }
#else
    LOG_WRN("recovery button alias sw0 not defined; button input disabled");
#endif

    LOG_INF("init");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_recovery_ui_enable_inputs(void) {
    g_input_events_enabled = true;
    LOG_INF("inputs enabled");
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
