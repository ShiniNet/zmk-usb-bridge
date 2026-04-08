#include <zephyr/ztest.h>

#include "zmk_usb_bridge/pairing_filter.h"

static void *pairing_filter_setup(void) {
    zassert_equal(zmk_usb_bridge_pairing_filter_init(), ZMK_USB_BRIDGE_STATUS_OK, NULL);
    return NULL;
}

ZTEST_SUITE(pairing_filter, NULL, pairing_filter_setup, NULL, NULL, NULL);

ZTEST(pairing_filter, test_name_allowlist_match_is_case_insensitive) {
    zassert_true(zmk_usb_bridge_pairing_filter_name_allowed("lalapadgen2"), NULL);
    zassert_true(zmk_usb_bridge_pairing_filter_name_allowed("ALPHA-KEYBOARD"), NULL);
}

ZTEST(pairing_filter, test_name_allowlist_rejects_null_or_unknown_name) {
    zassert_false(zmk_usb_bridge_pairing_filter_name_allowed(NULL), NULL);
    zassert_false(zmk_usb_bridge_pairing_filter_name_allowed("unknown-keyboard"), NULL);
}

ZTEST(pairing_filter, test_candidate_requires_connectable_hid_keyboard_and_name_match) {
    zmk_usb_bridge_pairing_candidate_t accepted = {
        .connectable = true,
        .has_hid_service = true,
        .has_keyboard_appearance = true,
        .local_name = "LalapadGen2",
    };
    zmk_usb_bridge_pairing_candidate_t missing_keyboard = accepted;
    zmk_usb_bridge_pairing_candidate_t rejected_name = accepted;

    missing_keyboard.has_keyboard_appearance = false;
    rejected_name.local_name = "different";

    zassert_true(zmk_usb_bridge_pairing_filter_accept_unbonded_candidate(&accepted), NULL);
    zassert_false(
        zmk_usb_bridge_pairing_filter_accept_unbonded_candidate(&missing_keyboard),
        NULL
    );
    zassert_false(
        zmk_usb_bridge_pairing_filter_accept_unbonded_candidate(&rejected_name),
        NULL
    );
}
