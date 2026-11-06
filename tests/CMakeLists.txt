# SPDX-License-Identifier: MIT OR Apache-2.0
add_executable(test_core test_core.c)
target_link_libraries(test_core deluge)
add_test(NAME core COMMAND test_core)

add_executable(test_discover test_discover.c)
target_link_libraries(test_discover deluge)
target_include_directories(test_discover PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_discover PRIVATE FIXDIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/asound")
add_test(NAME discover COMMAND test_discover)
set_tests_properties(discover PROPERTIES
    ENVIRONMENT "DELUGE_INPUT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/input;DELUGE_ASOUND_CARDS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/asound/cards")

add_executable(test_audio_convert test_audio_convert.c)
target_link_libraries(test_audio_convert deluge m)
target_include_directories(test_audio_convert PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME audio_convert COMMAND test_audio_convert)

add_executable(test_open test_open.c)
target_link_libraries(test_open deluge)
add_test(NAME open COMMAND test_open)
set_tests_properties(open PROPERTIES
    ENVIRONMENT "DELUGE_ASOUND_CARDS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/asound/internal-only")

add_executable(test_input_decode test_input_decode.c)
target_link_libraries(test_input_decode deluge)
target_include_directories(test_input_decode PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME input_decode COMMAND test_input_decode)

add_executable(test_pad_decode test_pad_decode.c)
target_link_libraries(test_pad_decode deluge)
target_include_directories(test_pad_decode PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME pad_decode COMMAND test_pad_decode)

set(LEDS_FIX ${CMAKE_CURRENT_BINARY_DIR}/leds-fixture)
file(MAKE_DIRECTORY ${LEDS_FIX}/deluge:indicator:5)
file(WRITE ${LEDS_FIX}/deluge:indicator:5/brightness "0\n")
file(MAKE_DIRECTORY ${LEDS_FIX}/deluge:gold1:3)
file(WRITE ${LEDS_FIX}/deluge:gold1:3/brightness "0\n")
file(MAKE_DIRECTORY ${LEDS_FIX}/deluge:sync)
file(WRITE ${LEDS_FIX}/deluge:sync/brightness "0\n")

add_executable(test_leds test_leds.c)
target_link_libraries(test_leds deluge)
target_include_directories(test_leds PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME leds COMMAND test_leds)
set_tests_properties(leds PROPERTIES
    ENVIRONMENT "DELUGE_LEDS_ROOT=${CMAKE_CURRENT_BINARY_DIR}/leds-fixture")

set(IIO_FIX ${CMAKE_CURRENT_BINARY_DIR}/iio-fixture)
file(MAKE_DIRECTORY ${IIO_FIX}/iio:device0)
file(WRITE ${IIO_FIX}/iio:device0/name "deluge-cv\n")
file(WRITE ${IIO_FIX}/iio:device0/out_voltage0_raw "0\n")
file(WRITE ${IIO_FIX}/iio:device0/out_voltage1_raw "0\n")
add_executable(test_cv test_cv.c)
target_link_libraries(test_cv deluge m)
target_include_directories(test_cv PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME cv COMMAND test_cv)
set_tests_properties(cv PROPERTIES ENVIRONMENT "DELUGE_IIO_ROOT=${IIO_FIX}")

set(USB_FIX ${CMAKE_CURRENT_BINARY_DIR}/usb-fixture)
file(MAKE_DIRECTORY ${USB_FIX})
file(WRITE ${USB_FIX}/role "peripheral\n")
add_executable(test_usb test_usb.c)
target_link_libraries(test_usb deluge)
target_include_directories(test_usb PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME usb COMMAND test_usb)
set_tests_properties(usb PROPERTIES ENVIRONMENT "DELUGE_USB_ROLE_PATH=${USB_FIX}/role")

add_executable(test_usb_uevent test_usb_uevent.c)
target_link_libraries(test_usb_uevent deluge)
target_include_directories(test_usb_uevent PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME usb_uevent COMMAND test_usb_uevent)

add_executable(test_usb_bind test_usb_bind.c)
target_link_libraries(test_usb_bind deluge)
target_include_directories(test_usb_bind PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_usb_bind PRIVATE FIXDIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/asound")
add_test(NAME usb_bind COMMAND test_usb_bind)
set_tests_properties(usb_bind PROPERTIES
    ENVIRONMENT "DELUGE_ASOUND_CARDS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/asound/internal-only")


# deluge-pic's refresh_time lives on the platform DEVICE, so the fixture mirrors
# the driver-bind directory shape (<driver>/<device>/refresh_time) that
# open_refresh_time() scans, rather than a fixed path.
set(PIC_FIX ${CMAKE_CURRENT_BINARY_DIR}/pic-fixture)
file(MAKE_DIRECTORY ${PIC_FIX}/e8007800.serial)
file(WRITE ${PIC_FIX}/e8007800.serial/refresh_time "0\n")

add_executable(test_pads_refresh test_pads_refresh.c)
target_link_libraries(test_pads_refresh deluge)
target_include_directories(test_pads_refresh PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME pads_refresh COMMAND test_pads_refresh)
set_tests_properties(pads_refresh PROPERTIES
    ENVIRONMENT "DELUGE_PIC_DRIVER_ROOT=${CMAKE_CURRENT_BINARY_DIR}/pic-fixture")
