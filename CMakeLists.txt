cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zmk_usb_bridge)

target_include_directories(app PRIVATE main/include)

target_sources(app PRIVATE
    main/app_main.c
    main/src/ble_manager.c
    main/src/bridge.c
    main/src/hog_client.c
    main/src/pairing_filter.c
    main/src/persist.c
    main/src/recovery_ui.c
    main/src/state_machine.c
    main/src/usb_bridge.c
)
