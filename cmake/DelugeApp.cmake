# SPDX-License-Identifier: MIT OR Apache-2.0
# deluge_add_app(<name> [LINKAGE STATIC|DYNAMIC] SOURCES <files...> [BUNDLE_LIBS <sofiles...>])
#
# One compile, two products: a bootable appliance image for the card's /APPS/,
# and the bare binary for /LINUX/APPS/. They are the same executable; the
# image is that executable baked into the base rootfs.
#
# Products, both built by `all`:
#   ${CMAKE_CURRENT_BINARY_DIR}/<NAME>.ELF              (target <name>-image)
#   ${CMAKE_CURRENT_BINARY_DIR}/card/LINUX/APPS/<name>  (target <name>-bare)
# where <NAME> is <name> upper-cased.
#
# LINKAGE defaults to STATIC. DYNAMIC links against libraries in the rootfs,
# with an RPATH of $ORIGIN/../lib/<name> for the app's own private prefix;
# BUNDLE_LIBS (DYNAMIC only) stages plugin/runtime .so files into that prefix
# and passes it to deluge-mkimage via --prefix, which implies --dynamic.
#
# BUNDLE_LIBS contract: each entry is staged with `cmake -E copy_if_different`,
# which keeps the basename it is given and renames nothing. The staged
# filename must equal the library's DT_SONAME, because that is the name
# dlopen()/DT_NEEDED looks for at that prefix, and the SD card is vfat: it
# cannot hold a symlink from the soname (libplugin.so.1) to a versioned real
# file (libplugin.so.1.2.3). For a library target built with
# VERSION/SOVERSION, pass $<TARGET_SONAME_FILE:tgt>, not $<TARGET_FILE:tgt>;
# the latter is the versioned real file, and staging it under its own name
# produces a prefix that dlopen/DT_NEEDED finds nothing in at run time. For an
# unversioned .so (no SOVERSION) the two are the same file, so either works.
#
# The private prefix lands at
#   ${CMAKE_CURRENT_BINARY_DIR}/card/LINUX/lib/<name>/*.so
# next to the bare binary, mirroring the card's own /LINUX/APPS and /LINUX/lib
# layout, so `cp -r .../card/LINUX /sd/` is a correct deployment and the build
# tree runs directly under qemu ($ORIGIN/../lib/<name> resolves the same way).
# The SD card is vfat and presents every file as mode 0755, so the launcher
# treats any file under APPS/ as launchable; the prefix must be a sibling of
# APPS/, never inside it, or the .so files would show up as apps.
#
# Packaging needs `deluge-mkimage`, shipped in the deluge-linux release bundle
# under tools/. It is looked up in $DELUGE_BASE/tools, $DELUGE_BASE/bin, then
# PATH, the first time an app is added. It is optional: without it the app
# executable still builds, and the image and bare targets are skipped with a
# status message.

function(deluge_add_app NAME)
    cmake_parse_arguments(ARG "" "LINKAGE" "SOURCES;BUNDLE_LIBS" ${ARGN})

    if(NOT ARG_LINKAGE)
        set(ARG_LINKAGE STATIC)
    endif()
    if(NOT ARG_LINKAGE STREQUAL "STATIC" AND NOT ARG_LINKAGE STREQUAL "DYNAMIC")
        message(FATAL_ERROR
            "deluge_add_app(${NAME}): LINKAGE must be STATIC or DYNAMIC, "
            "got '${ARG_LINKAGE}'")
    endif()
    if(ARG_BUNDLE_LIBS AND ARG_LINKAGE STREQUAL "STATIC")
        message(FATAL_ERROR
            "deluge_add_app(${NAME}): BUNDLE_LIBS needs LINKAGE DYNAMIC — a "
            "static app cannot dlopen (musl's static dlopen always fails).")
    endif()

    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE deluge)

    if(ARG_LINKAGE STREQUAL "STATIC")
        # Static by default. A dynamic binary is welded to the rootfs it was
        # built against; deluge-mkimage rejects one, so fail here not there.
        target_link_options(${NAME} PRIVATE -static)
    else()
        # $ORIGIN/../lib/<name> is correct in BOTH product forms, because the
        # app sits one directory below the prefix in each:
        #   /usr/bin/deluge-app     -> /usr/lib/<name>/      (baked image)
        #   /sd/LINUX/APPS/<name>   -> /sd/LINUX/lib/<name>/ (bare, on the card)
        set_target_properties(${NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib/${NAME}"
            BUILD_WITH_INSTALL_RPATH ON)
        # --export-dynamic: a dlopened plugin resolves callbacks back into the
        # host app, whose symbols are otherwise absent from .dynsym.
        target_link_options(${NAME} PRIVATE -Wl,--export-dynamic)
    endif()

    # Locate the packer: the bundle (DELUGE_BASE) first, then PATH. Optional;
    # without it only the executable is built.
    find_program(DELUGE_MKIMAGE deluge-mkimage
        PATHS "$ENV{DELUGE_BASE}/tools" "$ENV{DELUGE_BASE}/bin")

    if(NOT DELUGE_MKIMAGE)
        message(STATUS
            "deluge_add_app(${NAME}): deluge-mkimage not found — building the "
            "executable only (no .ELF/bare image). Set DELUGE_BASE to a bundle "
            "that ships deluge-mkimage to enable packaging.")
        return()
    endif()

    string(TOUPPER "${NAME}" UPPER)

    # card/LINUX mirrors the SD card's own /LINUX layout: APPS/ (scanned by
    # the launcher, so it must contain only launchable binaries) and lib/<name>/
    # (private prefixes) as siblings. A wholesale `cp -r card/LINUX /sd/` is
    # then correct by construction, and $ORIGIN/../lib/<name> resolves the
    # same way under qemu against this tree as it will on the card.
    set(CARD_LINUX_DIR ${CMAKE_CURRENT_BINARY_DIR}/card/LINUX)
    set(BARE_DIR ${CARD_LINUX_DIR}/APPS)
    set(PREFIX_DIR ${CARD_LINUX_DIR}/lib/${NAME})
    set(MKIMAGE_FLAGS "")

    if(ARG_LINKAGE STREQUAL "DYNAMIC")
        set(MKIMAGE_FLAGS --dynamic)
        if(ARG_BUNDLE_LIBS)
            # Flattened real files named by DT_SONAME, never symlinks (see
            # the BUNDLE_LIBS contract at the top of this file: the caller,
            # not this copy, is responsible for the staged filename being
            # the SONAME).
            #
            # The stamp file lives OUTSIDE PREFIX_DIR (as a sibling, in
            # CMAKE_CURRENT_BINARY_DIR), not inside it: deluge-mkimage's
            # --prefix indexes every file in PREFIX_DIR as a library
            # provider, so a stamp file staged inside it would be indexed
            # right alongside the real .so files and rejected as not a
            # parseable ELF. The prefix must contain only libraries.
            set(PREFIX_STAMP ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-prefix.stamp)
            add_custom_command(
                OUTPUT ${PREFIX_STAMP}
                # copy_if_different only ever adds/updates — it never removes
                # a file that's no longer in BUNDLE_LIBS. Without the clean,
                # dropping an entry from BUNDLE_LIBS leaves its stale .so
                # sitting in PREFIX_DIR, where deluge-mkimage's --prefix
                # still indexes it as a provider: seeded as a closure-walk
                # root (a possible spurious failure if it's now missing its
                # own deps), baked into the image, and copied to the card —
                # all for a library the build no longer intends to ship.
                COMMAND ${CMAKE_COMMAND} -E rm -rf ${PREFIX_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${PREFIX_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${ARG_BUNDLE_LIBS} ${PREFIX_DIR}
                COMMAND ${CMAKE_COMMAND} -E touch ${PREFIX_STAMP}
                DEPENDS ${ARG_BUNDLE_LIBS}
                COMMENT "Staging ${NAME}'s private prefix")
            add_custom_target(${NAME}-prefix DEPENDS ${PREFIX_STAMP})
            list(APPEND MKIMAGE_FLAGS --prefix ${PREFIX_DIR})
        endif()
    endif()

    set(APP_DEPENDS ${NAME})
    if(TARGET ${NAME}-prefix)
        list(APPEND APP_DEPENDS ${NAME}-prefix)
    endif()

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${UPPER}.ELF
        COMMAND ${DELUGE_MKIMAGE} $<TARGET_FILE:${NAME}> ${MKIMAGE_FLAGS}
                -o ${CMAKE_CURRENT_BINARY_DIR}/${UPPER}.ELF
        DEPENDS ${APP_DEPENDS}
        COMMENT "Packing ${UPPER}.ELF (copy to the card's /APPS/)")
    add_custom_target(${NAME}-image ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${UPPER}.ELF)

    add_custom_command(
        OUTPUT ${BARE_DIR}/${NAME}
        # mkimage -o does not create missing parent directories itself.
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BARE_DIR}
        COMMAND ${DELUGE_MKIMAGE} $<TARGET_FILE:${NAME}> --bare ${MKIMAGE_FLAGS}
                -o ${BARE_DIR}/${NAME}
        DEPENDS ${APP_DEPENDS}
        COMMENT "Validating ${NAME} (copy card/LINUX/ to the card's /LINUX/)")
    add_custom_target(${NAME}-bare ALL
        DEPENDS ${BARE_DIR}/${NAME})
endfunction()
