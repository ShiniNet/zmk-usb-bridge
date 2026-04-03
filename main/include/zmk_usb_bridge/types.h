#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ZMK_USB_BRIDGE_ROLE_UNKNOWN = 0,
    ZMK_USB_BRIDGE_ROLE_KEYBOARD_INPUT,
    ZMK_USB_BRIDGE_ROLE_CONSUMER_INPUT,
    ZMK_USB_BRIDGE_ROLE_MOUSE_INPUT,
    ZMK_USB_BRIDGE_ROLE_MOUSE_FEATURE,
    ZMK_USB_BRIDGE_ROLE_LED_OUTPUT,
} zmk_usb_bridge_report_role_t;

typedef enum {
    ZMK_USB_BRIDGE_REPORT_TYPE_UNKNOWN = 0,
    ZMK_USB_BRIDGE_REPORT_TYPE_INPUT = 1,
    ZMK_USB_BRIDGE_REPORT_TYPE_OUTPUT = 2,
    ZMK_USB_BRIDGE_REPORT_TYPE_FEATURE = 3,
} zmk_usb_bridge_report_type_t;

typedef enum {
    ZMK_USB_BRIDGE_STATE_BOOT = 0,
    ZMK_USB_BRIDGE_STATE_USB_READY,
    ZMK_USB_BRIDGE_STATE_PAIRING_SCAN,
    ZMK_USB_BRIDGE_STATE_SCANNING_KNOWN_DEVICE,
    ZMK_USB_BRIDGE_STATE_CONNECTING,
    ZMK_USB_BRIDGE_STATE_CONNECTED,
    ZMK_USB_BRIDGE_STATE_RECONNECTING_FAST,
    ZMK_USB_BRIDGE_STATE_RECONNECTING_BACKOFF,
    ZMK_USB_BRIDGE_STATE_RECOVERY_REQUIRED,
    ZMK_USB_BRIDGE_STATE_BOND_ERASING,
    ZMK_USB_BRIDGE_STATE_FATAL_ERROR,
} zmk_usb_bridge_state_t;

typedef enum {
    ZMK_USB_BRIDGE_EVENT_NONE = 0,
    ZMK_USB_BRIDGE_EVENT_USB_READY,
    ZMK_USB_BRIDGE_EVENT_BLE_SYNCED,
    ZMK_USB_BRIDGE_EVENT_PERSIST_READY_WITH_BOND,
    ZMK_USB_BRIDGE_EVENT_PERSIST_READY_NO_BOND,
    ZMK_USB_BRIDGE_EVENT_SCAN_STARTED,
    ZMK_USB_BRIDGE_EVENT_KNOWN_DEVICE_FOUND,
    ZMK_USB_BRIDGE_EVENT_PAIRING_CANDIDATE_FOUND,
    ZMK_USB_BRIDGE_EVENT_CONNECT_SUCCESS,
    ZMK_USB_BRIDGE_EVENT_CONNECT_FAILURE,
    ZMK_USB_BRIDGE_EVENT_SECURITY_READY,
    ZMK_USB_BRIDGE_EVENT_HID_READY,
    ZMK_USB_BRIDGE_EVENT_HID_FAILURE,
    ZMK_USB_BRIDGE_EVENT_DISCONNECTED,
    ZMK_USB_BRIDGE_EVENT_HOST_RESET,
    ZMK_USB_BRIDGE_EVENT_BOND_MISMATCH,
    ZMK_USB_BRIDGE_EVENT_METADATA_FAULT,
    ZMK_USB_BRIDGE_EVENT_BUTTON_SHORT_PRESS,
    ZMK_USB_BRIDGE_EVENT_BUTTON_LONG_PRESS,
    ZMK_USB_BRIDGE_EVENT_BOND_ERASE_COMPLETE,
} zmk_usb_bridge_event_type_t;

typedef enum {
    ZMK_USB_BRIDGE_EVENT_REASON_NONE = 0,
    ZMK_USB_BRIDGE_EVENT_REASON_CONNECT_CREATE_FAILED,
    ZMK_USB_BRIDGE_EVENT_REASON_SECURITY_FAILED,
    ZMK_USB_BRIDGE_EVENT_REASON_HID_SERVICE_MISSING,
    ZMK_USB_BRIDGE_EVENT_REASON_HID_REPORT_MISSING,
    ZMK_USB_BRIDGE_EVENT_REASON_HID_SUBSCRIBE_FAILED,
    ZMK_USB_BRIDGE_EVENT_REASON_DISCONNECTED_REMOTE,
    ZMK_USB_BRIDGE_EVENT_REASON_DISCONNECTED_LOCAL,
    ZMK_USB_BRIDGE_EVENT_REASON_DISCONNECTED_TIMEOUT,
    ZMK_USB_BRIDGE_EVENT_REASON_BOND_AUTH_MISMATCH,
    ZMK_USB_BRIDGE_EVENT_REASON_BOND_IDENTITY_MISMATCH,
    ZMK_USB_BRIDGE_EVENT_REASON_BOND_STORE_UNAVAILABLE,
    ZMK_USB_BRIDGE_EVENT_REASON_METADATA_CORRUPTED,
    ZMK_USB_BRIDGE_EVENT_REASON_METADATA_VERSION_MISMATCH,
    ZMK_USB_BRIDGE_EVENT_REASON_BLE_BOND_ERASE_FAILED,
    ZMK_USB_BRIDGE_EVENT_REASON_METADATA_ERASE_FAILED,
} zmk_usb_bridge_event_reason_t;

typedef enum {
    ZMK_USB_BRIDGE_CAPABILITY_NONE = 0,
    ZMK_USB_BRIDGE_CAPABILITY_KEYBOARD_INPUT = (1U << 0),
    ZMK_USB_BRIDGE_CAPABILITY_CONSUMER_INPUT = (1U << 1),
    ZMK_USB_BRIDGE_CAPABILITY_MOUSE_INPUT = (1U << 2),
    ZMK_USB_BRIDGE_CAPABILITY_MOUSE_FEATURE = (1U << 3),
    ZMK_USB_BRIDGE_CAPABILITY_LED_OUTPUT = (1U << 4),
} zmk_usb_bridge_capability_flags_t;

typedef struct {
    zmk_usb_bridge_event_type_t type;
    int32_t status_code;
    uint16_t conn_handle;
    uint16_t capability_flags;
    zmk_usb_bridge_event_reason_t reason;
    uint8_t reserved;
} zmk_usb_bridge_event_t;

typedef struct {
    uint16_t value_handle;
    uint16_t ccc_handle;
    uint8_t report_id;
    zmk_usb_bridge_report_type_t report_type;
    zmk_usb_bridge_report_role_t role;
    bool subscribed;
} zmk_usb_bridge_report_binding_t;

typedef struct {
    bool has_keyboard_input;
    bool has_consumer_input;
    bool has_mouse_input;
    bool has_mouse_feature;
    bool has_led_output;
} zmk_usb_bridge_hog_profile_t;

typedef struct {
    bool valid;
    uint8_t addr_type;
    uint8_t addr[6];
} zmk_usb_bridge_peer_addr_snapshot_t;

typedef struct {
    uint8_t metadata_version;
    zmk_usb_bridge_peer_addr_snapshot_t identity_snapshot;
    zmk_usb_bridge_peer_addr_snapshot_t last_peer_snapshot;
} zmk_usb_bridge_metadata_t;
