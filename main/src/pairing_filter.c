#include "zmk_usb_bridge/pairing_filter.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/sys/util.h>

enum {
    ZMK_USB_BRIDGE_ALLOWLIST_RAW_MAX = 192,
    ZMK_USB_BRIDGE_ALLOWLIST_NAME_MAX = 32,
    ZMK_USB_BRIDGE_ALLOWLIST_MAX_ENTRIES = 8,
};

static char g_allowlist_storage[ZMK_USB_BRIDGE_ALLOWLIST_RAW_MAX];
static const char *g_allowlist_entries[ZMK_USB_BRIDGE_ALLOWLIST_MAX_ENTRIES];
static size_t g_allowlist_count;

static bool equals_case_insensitive(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }

        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static char *trim_in_place(char *text) {
    if (text == NULL) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';
    return text;
}

zmk_usb_bridge_status_t zmk_usb_bridge_pairing_filter_init(void) {
    g_allowlist_count = 0;
    memset(g_allowlist_storage, 0, sizeof(g_allowlist_storage));
    memset(g_allowlist_entries, 0, sizeof(g_allowlist_entries));

    if (!IS_ENABLED(CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST_ENABLED)) {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    const char *configured = CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST;
    if (configured == NULL || configured[0] == '\0') {
        return ZMK_USB_BRIDGE_STATUS_OK;
    }

    const size_t configured_len = strnlen(configured, sizeof(g_allowlist_storage));
    if (configured_len >= sizeof(g_allowlist_storage)) {
        return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
    }

    memcpy(g_allowlist_storage, configured, configured_len + 1);

    char *cursor = g_allowlist_storage;
    while (*cursor != '\0' && g_allowlist_count < ZMK_USB_BRIDGE_ALLOWLIST_MAX_ENTRIES) {
        char *token = cursor;
        char *comma = strchr(cursor, ',');
        if (comma != NULL) {
            *comma = '\0';
            cursor = comma + 1;
        } else {
            cursor += strlen(cursor);
        }

        token = trim_in_place(token);
        if (token[0] == '\0') {
            continue;
        }

        if (strlen(token) >= ZMK_USB_BRIDGE_ALLOWLIST_NAME_MAX) {
            return ZMK_USB_BRIDGE_STATUS_SIZE_MISMATCH;
        }

        g_allowlist_entries[g_allowlist_count++] = token;
    }

    return ZMK_USB_BRIDGE_STATUS_OK;
}

bool zmk_usb_bridge_pairing_filter_name_allowlist_enabled(void) {
    return IS_ENABLED(CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST_ENABLED);
}

bool zmk_usb_bridge_pairing_filter_name_allowed(const char *local_name) {
    if (!zmk_usb_bridge_pairing_filter_name_allowlist_enabled()) {
        return true;
    }

    if (g_allowlist_count == 0) {
        return true;
    }

    if (local_name == NULL || local_name[0] == '\0') {
        return false;
    }

    for (size_t index = 0; index < g_allowlist_count; index++) {
        if (equals_case_insensitive(local_name, g_allowlist_entries[index])) {
            return true;
        }
    }

    return false;
}

bool zmk_usb_bridge_pairing_filter_accept_unbonded_candidate(
    const zmk_usb_bridge_pairing_candidate_t *candidate
) {
    if (candidate == NULL) {
        return false;
    }

    if (!candidate->connectable) {
        return false;
    }

    if (!candidate->has_hid_service) {
        return false;
    }

    if (!candidate->has_keyboard_appearance) {
        return false;
    }

    return zmk_usb_bridge_pairing_filter_name_allowed(candidate->local_name);
}
