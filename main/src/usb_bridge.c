#include "zmk_usb_bridge/usb_bridge.h"

#include "zmk_usb_bridge/hog_client.h"

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
#define ZMK_USB_BRIDGE_GEN_DESKTOP_RESOLUTION_MULTIPLIER_USAGE 0x48U
#define ZMK_USB_BRIDGE_MOUSE_RESOLUTION_MAX 0x0FU
#define ZMK_USB_BRIDGE_HID_GET_REPORT_TYPE_MASK 0xFF00U
#define ZMK_USB_BRIDGE_HID_GET_REPORT_ID_MASK 0x00FFU
#define ZMK_USB_BRIDGE_HID_REPORT_TYPE_INPUT 0x0100U
#define ZMK_USB_BRIDGE_HID_REPORT_TYPE_OUTPUT 0x0200U
#define ZMK_USB_BRIDGE_HID_REPORT_TYPE_FEATURE 0x0300U
#define ZMK_USB_BRIDGE_HID_USAGE16(lsb, msb) \
    HID_ITEM(HID_ITEM_TAG_USAGE, HID_ITEM_TYPE_LOCAL, 2), lsb, msb
#define ZMK_USB_BRIDGE_HID_PHYSICAL_MIN8(value) \
    HID_ITEM(HID_ITEM_TAG_PHYSICAL_MIN, HID_ITEM_TYPE_GLOBAL, 1), value
#define ZMK_USB_BRIDGE_HID_PHYSICAL_MAX8(value) \
    HID_ITEM(HID_ITEM_TAG_PHYSICAL_MAX, HID_ITEM_TYPE_GLOBAL, 1), value
#define ZMK_USB_BRIDGE_HID_PUSH HID_ITEM(10, HID_ITEM_TYPE_GLOBAL, 0)
#define ZMK_USB_BRIDGE_HID_POP HID_ITEM(11, HID_ITEM_TYPE_GLOBAL, 0)
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
    int16_t dx;
    int16_t dy;
    int16_t scroll_y;
    int16_t scroll_x;
} __packed;

struct zmk_usb_bridge_mouse_feature_report {
    uint8_t report_id;
    uint8_t resolution;
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
    HID_LOGICAL_MIN16(0x00, 0x80),
    HID_LOGICAL_MAX16(0xFF, 0x7F),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(2),
    HID_INPUT(0x06),
    HID_COLLECTION(HID_COLLECTION_LOGICAL),
    HID_USAGE(ZMK_USB_BRIDGE_GEN_DESKTOP_RESOLUTION_MULTIPLIER_USAGE),
    HID_LOGICAL_MIN8(0),
    HID_LOGICAL_MAX8(ZMK_USB_BRIDGE_MOUSE_RESOLUTION_MAX),
    ZMK_USB_BRIDGE_HID_PHYSICAL_MIN8(1),
    ZMK_USB_BRIDGE_HID_PHYSICAL_MAX8(16),
    HID_REPORT_SIZE(4),
    HID_REPORT_COUNT(1),
    ZMK_USB_BRIDGE_HID_PUSH,
    HID_FEATURE(0x02),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_WHEEL),
    HID_LOGICAL_MIN16(0x00, 0x80),
    HID_LOGICAL_MAX16(0xFF, 0x7F),
    ZMK_USB_BRIDGE_HID_PHYSICAL_MIN8(0),
    ZMK_USB_BRIDGE_HID_PHYSICAL_MAX8(0),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x06),
    HID_END_COLLECTION,
    HID_COLLECTION(HID_COLLECTION_LOGICAL),
    HID_USAGE(ZMK_USB_BRIDGE_GEN_DESKTOP_RESOLUTION_MULTIPLIER_USAGE),
    ZMK_USB_BRIDGE_HID_POP,
    HID_FEATURE(0x02),
    HID_USAGE_PAGE(ZMK_USB_BRIDGE_CONSUMER_USAGE_PAGE),
    ZMK_USB_BRIDGE_HID_USAGE16(
        ZMK_USB_BRIDGE_CONSUMER_AC_PAN_USAGE_LSB,
        ZMK_USB_BRIDGE_CONSUMER_AC_PAN_USAGE_MSB
    ),
    HID_LOGICAL_MIN16(0x00, 0x80),
    HID_LOGICAL_MAX16(0xFF, 0x7F),
    ZMK_USB_BRIDGE_HID_PHYSICAL_MIN8(0),
    ZMK_USB_BRIDGE_HID_PHYSICAL_MAX8(0),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_INPUT(0x06),
    HID_END_COLLECTION,
    HID_END_COLLECTION,
    HID_END_COLLECTION,
};

static const struct device *g_hid_dev;
static bool g_usb_configured;
static bool g_safe_state_pending;
static K_SEM_DEFINE(g_usb_in_ready, 1, 1);
static K_MUTEX_DEFINE(g_usb_lock);

static uint8_t encode_mouse_resolution(const zmk_usb_bridge_mouse_resolution_t *resolution) {
    if (resolution == NULL) {
        return 0U;
    }

    return (uint8_t)((MIN(resolution->hor_wheel, (uint8_t)ZMK_USB_BRIDGE_MOUSE_RESOLUTION_MAX)
                      << 4) |
                     MIN(resolution->wheel, (uint8_t)ZMK_USB_BRIDGE_MOUSE_RESOLUTION_MAX));
}

static zmk_usb_bridge_status_t decode_mouse_feature_report(
    const struct zmk_usb_bridge_mouse_feature_report *report,
    zmk_usb_bridge_mouse_resolution_t *resolution
) {
    if (report == NULL || resolution == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    resolution->wheel = report->resolution & 0x0F;
    resolution->hor_wheel = (report->resolution >> 4) & 0x0F;
    return ZMK_USB_BRIDGE_STATUS_OK;
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

static zmk_usb_bridge_status_t send_report_locked(
    const void *report,
    size_t report_len,
    const char *label
) {
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

static zmk_usb_bridge_status_t flush_safe_state_locked(void) {
    const struct zmk_usb_bridge_keyboard_report keyboard = {
        .report_id = ZMK_USB_BRIDGE_KEYBOARD_REPORT_ID,
    };
    const struct zmk_usb_bridge_consumer_report consumer = {
        .report_id = ZMK_USB_BRIDGE_CONSUMER_REPORT_ID,
        .usage = 0U,
    };
    const struct zmk_usb_bridge_mouse_report mouse = {
        .report_id = ZMK_USB_BRIDGE_MOUSE_REPORT_ID,
    };
    zmk_usb_bridge_status_t status;

    if (!g_safe_state_pending || !g_usb_configured) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    status = send_report_locked(&keyboard, sizeof(keyboard), "keyboard release");
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = send_report_locked(&consumer, sizeof(consumer), "consumer release");
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    status = send_report_locked(&mouse, sizeof(mouse), "mouse release");
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return status;
    }

    g_safe_state_pending = false;
    return ZMK_USB_BRIDGE_STATUS_OK;
}

static void zmk_usb_bridge_usb_ready_cb(const struct device *dev) {
    ARG_UNUSED(dev);
    k_sem_give(&g_usb_in_ready);
}

static int zmk_usb_bridge_usb_get_report_cb(
    const struct device *dev,
    struct usb_setup_packet *setup,
    int32_t *len,
    uint8_t **data
) {
    static struct zmk_usb_bridge_mouse_feature_report mouse_feature;
    const zmk_usb_bridge_mouse_resolution_t resolution =
        zmk_usb_bridge_hog_client_mouse_resolution();

    ARG_UNUSED(dev);

    if ((setup->wValue & ZMK_USB_BRIDGE_HID_GET_REPORT_TYPE_MASK) !=
        ZMK_USB_BRIDGE_HID_REPORT_TYPE_FEATURE) {
        return -ENOTSUP;
    }

    if ((setup->wValue & ZMK_USB_BRIDGE_HID_GET_REPORT_ID_MASK) != ZMK_USB_BRIDGE_MOUSE_REPORT_ID) {
        return -ENOTSUP;
    }

    mouse_feature.report_id = ZMK_USB_BRIDGE_MOUSE_REPORT_ID;
    mouse_feature.resolution = encode_mouse_resolution(&resolution);
    *len = sizeof(mouse_feature);
    *data = (uint8_t *)&mouse_feature;
    return 0;
}

static int zmk_usb_bridge_usb_set_report_cb(
    const struct device *dev,
    struct usb_setup_packet *setup,
    int32_t *len,
    uint8_t **data
) {
    struct zmk_usb_bridge_mouse_feature_report *report;
    zmk_usb_bridge_mouse_resolution_t resolution;
    zmk_usb_bridge_status_t status;

    ARG_UNUSED(dev);

    if ((setup->wValue & ZMK_USB_BRIDGE_HID_GET_REPORT_TYPE_MASK) !=
        ZMK_USB_BRIDGE_HID_REPORT_TYPE_FEATURE) {
        return -ENOTSUP;
    }

    if ((setup->wValue & ZMK_USB_BRIDGE_HID_GET_REPORT_ID_MASK) != ZMK_USB_BRIDGE_MOUSE_REPORT_ID) {
        return -ENOTSUP;
    }

    if (*len != (int32_t)sizeof(*report)) {
        return -EINVAL;
    }

    report = (struct zmk_usb_bridge_mouse_feature_report *)*data;
    status = decode_mouse_feature_report(report, &resolution);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        return -EINVAL;
    }

    status = zmk_usb_bridge_hog_client_set_mouse_resolution(&resolution);
    if (status != ZMK_USB_BRIDGE_STATUS_OK) {
        LOG_WRN("mouse feature set failed status=%d", status);
        return -EIO;
    }

    LOG_INF(
        "mouse resolution requested by host wheel=%u hor_wheel=%u",
        resolution.wheel,
        resolution.hor_wheel
    );
    return 0;
}

static const struct hid_ops zmk_usb_bridge_hid_ops = {
    .get_report = zmk_usb_bridge_usb_get_report_cb,
    .set_report = zmk_usb_bridge_usb_set_report_cb,
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
    g_safe_state_pending = true;
    LOG_INF("USB HID spike initialized");
    return ZMK_USB_BRIDGE_STATUS_OK;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_keyboard(const zmk_usb_bridge_keyboard_body_t *body) {
    struct zmk_usb_bridge_keyboard_report report;
    zmk_usb_bridge_status_t status;

    if (body == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    k_mutex_lock(&g_usb_lock, K_FOREVER);
    status = flush_safe_state_locked();
    if (status != ZMK_USB_BRIDGE_STATUS_OK || !g_usb_configured) {
        k_mutex_unlock(&g_usb_lock);
        return status;
    }

    report.report_id = ZMK_USB_BRIDGE_KEYBOARD_REPORT_ID;
    report.modifiers = body->modifiers;
    report.reserved = body->reserved;
    memcpy(report.keys, body->keys, sizeof(report.keys));
    status = send_report_locked(&report, sizeof(report), "keyboard");
    k_mutex_unlock(&g_usb_lock);
    return status;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_consumer(const zmk_usb_bridge_consumer_body_t *body) {
    struct zmk_usb_bridge_consumer_report report;
    zmk_usb_bridge_status_t status;

    if (body == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    k_mutex_lock(&g_usb_lock, K_FOREVER);
    status = flush_safe_state_locked();
    if (status != ZMK_USB_BRIDGE_STATUS_OK || !g_usb_configured) {
        k_mutex_unlock(&g_usb_lock);
        return status;
    }

    report.report_id = ZMK_USB_BRIDGE_CONSUMER_REPORT_ID;
    report.usage = sys_cpu_to_le16(body->usage);
    status = send_report_locked(&report, sizeof(report), "consumer");
    k_mutex_unlock(&g_usb_lock);
    return status;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_send_mouse(const zmk_usb_bridge_mouse_body_t *body) {
    struct zmk_usb_bridge_mouse_report report;
    zmk_usb_bridge_status_t status;

    if (body == NULL) {
        return ZMK_USB_BRIDGE_STATUS_INVALID_ARGUMENT;
    }

    k_mutex_lock(&g_usb_lock, K_FOREVER);
    status = flush_safe_state_locked();
    if (status != ZMK_USB_BRIDGE_STATUS_OK || !g_usb_configured) {
        k_mutex_unlock(&g_usb_lock);
        return status;
    }

    report.report_id = ZMK_USB_BRIDGE_MOUSE_REPORT_ID;
    report.buttons = body->buttons & BIT_MASK(5);
    report.dx = sys_cpu_to_le16((uint16_t)body->dx);
    report.dy = sys_cpu_to_le16((uint16_t)body->dy);
    report.scroll_y = sys_cpu_to_le16((uint16_t)body->scroll_y);
    report.scroll_x = sys_cpu_to_le16((uint16_t)body->scroll_x);
    status = send_report_locked(&report, sizeof(report), "mouse");
    k_mutex_unlock(&g_usb_lock);
    return status;
}

zmk_usb_bridge_status_t zmk_usb_bridge_usb_bridge_release_all(void) {
    zmk_usb_bridge_status_t status;

    k_mutex_lock(&g_usb_lock, K_FOREVER);
    g_safe_state_pending = true;
    status = flush_safe_state_locked();
    k_mutex_unlock(&g_usb_lock);
    return status;
}
