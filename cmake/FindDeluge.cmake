# SPDX-License-Identifier: MIT OR Apache-2.0
# find_package(Deluge REQUIRED) module, for apps built as a subdirectory of
# libdeluge's own CMakeLists.txt (examples/app and examples/dlopen_demo).
#
# The parent project has already defined the `deluge` target before
# descending into the app's directory. This module makes that target and
# deluge_add_app() visible under the name find_package() expects, and fails
# with a clear message if the target is missing instead of producing a
# confusing link error later.
#
# An app in its own repository does not need this module: it pulls libdeluge
# in with FetchContent, which defines `deluge` and deluge_add_app() directly
# (see docs/building-an-app.md).
#
# This is a module (Find<Name>.cmake on CMAKE_MODULE_PATH) rather than a
# package config file because the cross toolchain sets
# CMAKE_FIND_ROOT_PATH_MODE_PACKAGE to ONLY, which confines Config-mode
# searches to the target sysroot; this file lives on the host next to the
# libdeluge sources.
include("${CMAKE_CURRENT_LIST_DIR}/DelugeApp.cmake")

if(NOT TARGET deluge)
    message(FATAL_ERROR
        "Deluge: no 'deluge' target in scope. Build your app as a subdirectory "
        "of the libdeluge root CMakeLists.txt (see docs/building-an-app.md), "
        "not as a fully standalone configure.")
endif()

set(Deluge_FOUND TRUE)
