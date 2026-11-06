# SPDX-License-Identifier: MIT OR Apache-2.0
cmake_minimum_required(VERSION 3.21)
project(deluge_example_app C)

# Built as a subdirectory of libdeluge's root CMakeLists.txt when
# cross-compiling:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DELUGE_BASE/DelugeToolchain.cmake
# An app in its own repository uses FetchContent instead; see
# docs/building-an-app.md.
find_package(Deluge REQUIRED)

deluge_add_app(example SOURCES main.c)
