# SPDX-License-Identifier: MIT OR Apache-2.0
cmake_minimum_required(VERSION 3.21)

# When DELUGE_BASE is set, generate cmake/DelugeToolchain.cmake from the
# bundle. This runs before project() so that a -DCMAKE_TOOLCHAIN_FILE=
# cmake/DelugeToolchain.cmake on the same command line finds a freshly
# generated file when project() loads it; it re-runs on every configure, so
# the path tracks DELUGE_BASE. The bundle also ships an equivalent
# $DELUGE_BASE/DelugeToolchain.cmake, which works for in-tree builds too.
#
# PROJECT_IS_TOP_LEVEL is not set before project(), so this condition is its
# equivalent: CMAKE_SOURCE_DIR equals CMAKE_CURRENT_SOURCE_DIR exactly when
# this listfile is the outermost one, that is, when libdeluge is not pulled in
# by FetchContent or add_subdirectory. A FetchContent consumer supplies the
# bundle's toolchain file before its own project(), so nothing is generated
# for it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND DEFINED ENV{DELUGE_BASE})
    set(DELUGE_TOOLCHAIN "$ENV{DELUGE_BASE}/toolchain")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DelugeToolchain.cmake.in
                   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DelugeToolchain.cmake @ONLY)
endif()

project(libdeluge VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

# Both default ON, so a plain in-tree build (and CI) builds the examples and
# the ctest suite. A packaging build (for example the deluge-linux Buildroot
# package) sets both OFF to build only the `deluge` library and its install
# rules (libdeluge.a, headers, deluge.pc).
option(LIBDELUGE_EXAMPLES "Build the in-tree C example apps" ON)
option(LIBDELUGE_TESTS "Build the ctest suite" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(Threads REQUIRED)

add_library(deluge STATIC src/core.c src/discover.c src/audio_convert.c src/audio.c src/midi.c src/input_decode.c src/input.c src/leds.c src/display.c src/cv.c src/sram.c src/usb.c src/usb_uevent.c src/usb_midi.c src/usb_audio.c src/jacks.c)
target_include_directories(deluge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(deluge PUBLIC ${ALSA_LIBRARIES} Threads::Threads m)
target_include_directories(deluge PUBLIC ${ALSA_INCLUDE_DIRS})

install(TARGETS deluge ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

# deluge_add_app() must be visible to every consumer, in-tree or fetched with
# FetchContent, so this include is unconditional. Everything after it (the
# examples, the pkg-config file, the tests) is only for building libdeluge
# itself and is gated on PROJECT_IS_TOP_LEVEL: a FetchContent consumer gets
# just the `deluge` target and deluge_add_app().
include(${CMAKE_CURRENT_LIST_DIR}/cmake/DelugeApp.cmake)

if(PROJECT_IS_TOP_LEVEL)
    configure_file(deluge.pc.in deluge.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/deluge.pc DESTINATION lib/pkgconfig)

    # examples/app's find_package(Deluge REQUIRED) resolves cmake/FindDeluge.cmake
    # from here. Module mode, not Config mode: see FindDeluge.cmake for why.
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

    if(LIBDELUGE_EXAMPLES)
        add_executable(sine_out examples/sine_out.c)
        target_link_libraries(sine_out deluge m)

        add_executable(midi_echo examples/midi_echo.c)
        target_link_libraries(midi_echo deluge)

        add_executable(event_dump examples/event_dump.c)
        target_link_libraries(event_dump deluge)

        add_executable(display_demo examples/display_demo.c)
        target_link_libraries(display_demo deluge)

        add_executable(deluge-selftest examples/selftest.c)
        target_link_libraries(deluge-selftest deluge m)

        add_executable(cv_demo examples/cv_demo.c)
        target_link_libraries(cv_demo deluge)

        add_executable(usb_role examples/usb_role.c)
        target_link_libraries(usb_role deluge)

        add_executable(usb_data examples/usb_data.c)
        target_link_libraries(usb_data deluge)

        add_executable(usb_audio_measure examples/usb_audio_measure.c)
        target_link_libraries(usb_audio_measure deluge)

        add_executable(usb_midi_dump examples/usb_midi_dump.c)
        target_link_libraries(usb_midi_dump deluge)

        # These use deluge_add_app(), so only build them when cross-compiling:
        # deluge-mkimage rejects a host binary (wrong e_machine), and there is
        # no rootfs to pack it into on the host.
        if(CMAKE_CROSSCOMPILING)
            add_subdirectory(examples/app)
            add_subdirectory(examples/dlopen_demo)
        endif()
    endif()

    if(LIBDELUGE_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
