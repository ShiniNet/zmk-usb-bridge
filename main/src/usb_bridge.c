#include "zmk_usb_bridge/usb_bridge.h"

#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(zub_usb, LOG_LEVEL_INF);

#define ZMK_USB_BRIDGE_HID_DEV_NAME "HID_0"
#define ZMK_USB_BRIDGE_KEYBOARD_REPORT_ID 1U
#define ZMK_USB_BRIDGE_CONSUMER_REPORT_ID 2U
#define ZMK_USB_BRIDGE_MOUSE_REPORT_ID 3U
#define ZMK_USB_BRIDGE_CONSUMER_USAGE_PAGE 0x0CU
#define ZMK_USB_BRIDGE_CONSUMER_CONTROL_USAGE 0x01U
#define ZMK_USB_BRIDGE_CONSUMER_AC_PAN_USAGE_LSB 0x38U
#define ZMK_USB_BRIDGE_CONSUMER_AC_PAN_USAGE_MSB 0x02U
#define ZMK_USB_BRIDGE_HID_USAGE16(lsb, msb) \
    HID_ITEM(HID_ITEM_TAG_USAGE, HID_ITEM_TYPE_LOCAL, 2), lsb, msb
#define ZMK_USB_BRIDGE_SEND_TIMEOUT K_MSEC(50)

struct zmk_usb_bridge_keyboard_report {
    uint8_t report_id;
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} __packed;

struct zmk_usb_bridge_consumer_report {
    uint8_t report_id;
    uint16_t usage;
} __packed;

struct zmk_usb_bridge_mouse_report {
    uint8_t report_id;
    uint8_t buttons;
    int8_t dx;
    int8_t dy;
    int8_t scroll_y;
    int8_t scroll_x;
} __packed;

static const uint8_t zmk_usb_bridge_hid_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_USB_BRIDGE_KEYBOARD_REPORT_ID),
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),
    HID_USAGE_MIN8(0xE0),
    HID_USAGE_MAX8(0xE7),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(1),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(8),
    HID_INPUT(0x02),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x03),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(5),
    HID_USAGE_PAGE(HID_USAGE_GEN_LEDS),
    HID_USAGE_MIN8(1),
    HID_USAGE_MAX8(5),
    HID_OUTPUT(0x02),
    HID_REPORT_SIZE(3),
    HID_REPORT_COUNT(1),
    HID_OUTPUT(0x03),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(6),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(101),
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),
    HID_USAGE_MIN8(0),
    HID_USAGE_MAX8(101),
    HID_INPUT(0x00),
    HID_END_COLLECTION,

    HID_USAGE_PAGE(ZMK_USB_BRIDGE_CONSUMER_USAGE_PAGE),
    HID_USAGE(ZMK_USB_BRIDGE_CONSUMER_CONTROL_USAGE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_USB_BRIDGE_CONSUMER_REPORT_ID),
    HID_LOGICAL_MIN16(0x00, 0x00),
    HID_LOGICAL_MAX16(0xFF, 0x03),
    HID_USAGE_MIN16(0x00, 0x00),
    HID_USAGE_MAX16(0xFF, 0x03),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x00),
    HID_END_COLLECTION,

    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_MOUSE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_USB_BRIDGE_MOUSE_REPORT_ID),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),
    HID_COLLECTION(HID_COLLECTION_PHYSICAL),
    HID_USAGE_PAGE(HID_USAGE_GEN_BUTTON),
    HID_USAGE_MIN8(1),
    HID_USAGE_MAX8(5),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(1),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(5),
    HID_INPUT(0x02),
    HID_REPORT_SIZE(3),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x03),
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_X),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_WHEEL),
    HID_LOGICAL_MIN8(-127),
    HID_LOGICAL_MAX8(127),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(3),
    HID_INPUT(0x06),
    HID_USAGE_PAGE(ZMK_USB_BRIDGE_CONSUMER_USAGE_PAGE),
    ZMK_USB_BRIDGE_HID_USAGE16(
        ZMK_USB_BRIDGE_CONSUMER_AC_PAN_USAGE_LSB,
        ZMK_USB_BRIDGE_CONSUMER_AC_PAN_USAGE_MSB
    ),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x06),
    HID_END_COLLECTION,
    HID_END_COLLECTION,
};

static const struct device *g_hid_dev;
static bool g_usb_configured;
static K_SEM_DEFINE(g_usb_in_ready, 1, 1);

static int8_t saturate_to_i8(int16_t value) {
    return (int8_t)CLAMP((int)value, (int)INT8_MIN, (int)INT8_MAX);
}

static zmk_usb_bridge_status_t map_usb_error(int err) {
    switch (err) {
    case 0:
        return ZMK_USB_BRIDGE_STATUS_OK;
    case -EINVAL:
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    case -ENOMEM:
        return ZMK_USB_BRIDGE_STATUS_NO_MEMORY;
    case -EAGAIN:
    case -EBUSY:
        return ZMK_USB_BRIDGE_STATUS_QUEUE_FULL;
    default:
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }
}

static zmk_usb_bridge_status_t send_report(const void *report, size_t report_len, const char *label) {
    uint32_t wrote = 0;
    int ret;

    if (!g_usb_configured) {
        LOG_DBG("skip %s report while host is not configured", label);
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    if (k_sem_take(&g_usb_in_ready, ZMK_USB_BRIDGE_SEND_TIMEOUT) != 0) {
        LOG_WRN("%s report timed out waiting for IN endpoint", label);
        return ZMK_USB_BRIDGE_STATUS_QUEUE_FULL;
    }

    ret = hid_int_ep_write(g_hid_dev, report, report_len, &wrote);
    if (ret != 0) {
        k_sem_give(&g_usb_in_ready);
        LOG_WRN("%s report submission failed err=%d", label, ret);
        return map_usb_error(ret);
    }

    if (wrote != report_len) {
        k_sem_give(&g_usb_in_ready);
        LOG_WRN("%s report short write wrote=%u expected=%u", label, wrote, (unsigned int)report_len);
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

static void zmk_usb_bridge_usb_ready_cb(const struct device *dev) {
    ARG_UNUSED(dev);
    k_sem_give(&g_usb_in_ready);
}

static const struct hid_ops zmk_usb_bridge_hid_ops = {
    .int_in_ready = zmk_usb_bridge_usb_ready_cb,
};

static void zmk_usb_bridge_usb_status_cb(enum usb_dc_status_code status, const uint8_t *param) {
    ARG_UNUSED(param);

    switch (status) {
    case USB_DC_CONFIGURED:
        if (!g_usb_configured) {
            LOG_INF("USB HID configured");
        }
        g_usb_configured = true;
        k_sem_give(&g_usb_in_ready);
        break;
    case USB_DC_RESET:
    case USB_DC_DISCONNECTED:
        if (g_usb_configured) {
            LOG_INF("USB HID deconfigured status=%d", status);
        }
        g_usb_configured = false;
        k_sem_reset(&g_usb_in_ready);
        k_sem_give(&g_usb_in_ready);
        break;
    default:
        break;
    }
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_init(void) {
    int ret;

    g_hid_dev = device_get_binding(ZMK_USB_BRIDGE_HID_DEV_NAME);
    if (g_hid_dev == NULL) {
        LOG_ERR("failed to get USB HID device %s", ZMK_USB_BRIDGE_HID_DEV_NAME);
        return ZMK_USB_BRIDGE_STATUS_INVALID_STATE;
    }

    usb_hid_register_device(
        g_hid_dev,
        zmk_usb_bridge_hid_report_desc,
        sizeof(zmk_usb_bridge_hid_report_desc),
        &zmk_usb_bridge_hid_ops
    );

    ret = usb_hid_set_proto_code(g_hid_dev, HID_BOOT_IFACE_CODE_NONE);
    if (ret != 0) {
        LOG_WRN("failed to set HID protocol code err=%d", ret);
    }

    ret = usb_hid_init(g_hid_dev);
    if (ret != 0) {
        LOG_ERR("usb_hid_init failed err=%d", ret);
        return map_usb_error(ret);
    }

    ret = usb_enable(zmk_usb_bridge_usb_status_cb);
    if (ret != 0) {
        LOG_ERR("usb_enable failed err=%d", ret);
        return map_usb_error(ret);
    }

    g_usb_configured = false;
    LOG_INF("USB HID spike initialized");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_keyboard(const zmk_usb_bridge_keyboard_body_t *body) {
    struct zmk_usb_bridge_keyboard_report report;

    if (body == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    report.report_id = ZMK_USB_BRIDGE_KEYBOARD_REPORT_ID;
    report.modifiers = body->modifiers;
    report.reserved = body->reserved;
    memcpy(report.keys, body->keys, sizeof(report.keys));
    return send_report(&report, sizeof(report), "keyboard");
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_consumer(const zmk_usb_bridge_consumer_body_t *body) {
    struct zmk_usb_bridge_consumer_report report;

    if (body == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    report.report_id = ZMK_USB_BRIDGE_CONSUMER_REPORT_ID;
    report.usage = sys_cpu_to_le16(body->usage);
    return send_report(&report, sizeof(report), "consumer");
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_mouse(const zmk_usb_bridge_mouse_body_t *body) {
    struct zmk_usb_bridge_mouse_report report;

    if (body == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    report.report_id = ZMK_USB_BRIDGE_MOUSE_REPORT_ID;
    report.buttons = body->buttons & BIT_MASK(5);
    report.dx = saturate_to_i8(body->dx);
    report.dy = saturate_to_i8(body->dy);
    report.scroll_y = saturate_to_i8(body->scroll_y);
    report.scroll_x = saturate_to_i8(body->scroll_x);
    return send_report(&report, sizeof(report), "mouse");
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_release_all(void) {
    const zmk_usb_bridge_keyboard_body_t keyboard = {0};
    const zmk_usb_bridge_consumer_body_t consumer = {0};
    const zmk_usb_bridge_mouse_body_t mouse = {0};
    zmk_usb_bridge_status_t status;

    status = zmk_usb_bridge_usb_bridge_send_keyboard(&keyboard);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = zmk_usb_bridge_usb_bridge_send_consumer(&consumer);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    return zmk_usb_bridge_usb_bridge_send_mouse(&mouse);
}
