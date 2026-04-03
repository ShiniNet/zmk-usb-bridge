cmake_minimum_required(VERSION 3.20.0)

set(ZMK_USB_BRIDGE_DEFAULT_USER_CONF
    ${CMAKE_CURRENT_SOURCE_DIR}/config/user.conf)

if(DEFINED EXTRA_CONF_FILE)
    set(EXTRA_CONF_FILE
        "${ZMK_USB_BRIDGE_DEFAULT_USER_CONF};${EXTRA_CONF_FILE}")
else()
    set(EXTRA_CONF_FILE ${ZMK_USB_BRIDGE_DEFAULT_USER_CONF})
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zmk_usb_bridge)

target_include_directories(app PRIVATE main/include)

target_sources(app PRIVATE
    main/app_main.c
    main/src/startup.c
    main/src/ble_connection.c
    main/src/ble_manager.c
    main/src/ble_reconnect.c
    main/src/ble_runtime.c
    main/src/ble_scan.c
    main/src/bridge.c
    main/src/hog_client.c
    main/src/pairing_filter.c
    main/src/persist.c
    main/src/recovery_ui.c
    main/src/state_machine.c
    main/src/usb_bridge.c
)
