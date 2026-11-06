# SPDX-License-Identifier: MIT OR Apache-2.0
cmake_minimum_required(VERSION 3.21)
project(deluge_dlopen_demo C)

find_package(Deluge REQUIRED)

# SONAME must match the dlopen() string and the staged filename: the card is
# vfat, so there is no libdemoplugin.so -> .so.1 symlink to fall back on.
add_library(demoplugin SHARED plugin.c)
set_target_properties(demoplugin PROPERTIES
    OUTPUT_NAME demoplugin
    SOVERSION 1
    VERSION 1)

deluge_add_app(dlopen_demo
    LINKAGE     DYNAMIC
    SOURCES     main.c
    BUNDLE_LIBS $<TARGET_SONAME_FILE:demoplugin>)

add_dependencies(dlopen_demo demoplugin)
