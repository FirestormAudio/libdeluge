# Building an app

An app built against libdeluge produces two things from one compile:

| product | where it goes on the SD card | what it is |
| --- | --- | --- |
| `MYAPP.ELF` | `/APPS/` | A self-contained image: kernel, rootfs and your app. Launch it from the Deluge's boot menu. |
| `card/LINUX/APPS/myapp` | `/LINUX/APPS/` | The bare static binary, for running inside a generic deluge-linux image. |

A launcher that lists and runs `/LINUX/APPS/` from the Deluge's own UI is not
released yet. Until it is, run a bare app from the `platform` image's shell
(log in as `root` over USB serial or `ssh root@192.168.7.2`; the card is
mounted at `/sd`):

```sh
/sd/LINUX/APPS/myapp
```

The image name is the target name upper-cased. Buildroot never runs: your app
is compiled against a **base bundle**, a prebuilt kernel, rootfs and toolchain
published by deluge-linux, and baked into a copy of that rootfs.

## Get the bundle

Download `deluge-base-<version>.tar.zst` from the
[deluge-linux releases](https://github.com/FirestormAudio/deluge-linux/releases),
unpack it, and point `DELUGE_BASE` at the result:

```sh
tar --zstd -xf deluge-base-<version>.tar.zst
export DELUGE_BASE=$PWD/deluge-base-<version>
```

The bundle contains the cross toolchain, a relocatable
`DelugeToolchain.cmake`, the rootfs archives, and `tools/deluge-mkimage`, which
packs the products above. Without `deluge-mkimage`, `deluge_add_app()` still
builds the executable but skips both products, with a status message.

A bundle built locally from a deluge-linux checkout (`make bundle`) works the
same way; point `DELUGE_BASE` at it to test a kernel or rootfs change without
cutting a release.

## Your own project (FetchContent)

An app in its own repository pulls libdeluge in with `FetchContent`. When
libdeluge is not the top-level project it defines only the `deluge` library
target and the `deluge_add_app()` function; its examples, tests and
`deluge.pc` are not built.

The cross toolchain comes from the bundle, not from libdeluge, because the
toolchain file has to load before your project's `project()` call, which is
before libdeluge has been fetched.

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(myapp C)

include(FetchContent)
FetchContent_Declare(deluge
    GIT_REPOSITORY https://github.com/FirestormAudio/libdeluge.git
    GIT_TAG        v0.1.0)               # or SOURCE_DIR /path/to/checkout for local dev
FetchContent_MakeAvailable(deluge)

deluge_add_app(myapp SOURCES main.c)
```

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DELUGE_BASE/DelugeToolchain.cmake
cmake --build build
```

This produces `build/MYAPP.ELF` and `build/card/LINUX/APPS/myapp`. Both are
part of the default build; the individual targets are `myapp-image` and
`myapp-bare`.

[CPM](https://github.com/cpm-cmake/CPM.cmake) wraps FetchContent and works the
same way; replace the `FetchContent_Declare` / `FetchContent_MakeAvailable`
pair with one call:

```cmake
include(CPM.cmake)
CPMAddPackage(
    NAME deluge
    GIT_REPOSITORY https://github.com/FirestormAudio/libdeluge.git
    GIT_TAG        v0.1.0)

deluge_add_app(myapp SOURCES main.c)
```

## Inside a libdeluge checkout

This is how `examples/app` and `examples/dlopen_demo` build: the app's
directory is added as a subdirectory of libdeluge's root `CMakeLists.txt`,
which defines the `deluge` target first. In the app, `find_package(Deluge
REQUIRED)` (see `cmake/FindDeluge.cmake`) makes that target and
`deluge_add_app()` available. These apps are only built when cross-compiling.

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DELUGE_BASE/DelugeToolchain.cmake
cmake --build build
```

The example app's target is `example`, so this produces
`build/examples/app/EXAMPLE.ELF` and
`build/examples/app/card/LINUX/APPS/example`.

## Installing on the Deluge

- Image: copy `MYAPP.ELF` to the SD card's `/APPS/` and launch it from the
  Deluge's boot menu. deluge-linux's
  [boot.md](https://github.com/FirestormAudio/deluge-linux/blob/main/docs/boot.md)
  describes the boot menu and how to upload an ELF over USB instead.
- Bare binary: copy the whole `card/LINUX` directory onto the card, so that
  `card/LINUX/APPS/myapp` lands at `/LINUX/APPS/myapp`:

  ```sh
  cp -r build/card/LINUX /path/to/sd/
  ```

## Linkage

Apps are static by default. `deluge_add_app()` passes `-static`, and
`deluge-mkimage` rejects a dynamic binary unless the app opts in: a dynamic
binary is bound to the rootfs it was built against and fails with no console
output when the two drift apart.

An app that needs `dlopen`, such as a plugin host, opts in:

```cmake
deluge_add_app(myapp
    LINKAGE     DYNAMIC
    SOURCES     main.c
    BUNDLE_LIBS ${MY_PLUGIN_SO})
```

A static musl binary cannot `dlopen` at all: musl's static libc ships a stub
that always returns NULL with "Dynamic loading not supported". It links
without a warning and fails on the device.

With `LINKAGE DYNAMIC`, `deluge-mkimage` resolves the app's own `DT_NEEDED`
closure against the rootfs it packs into, and separately each `BUNDLE_LIBS`
library's own `DT_NEEDED` closure, since a `dlopen`ed plugin never appears in
the app's `DT_NEEDED`. A failure in either fails the build. It checks that
libraries resolve; `deluge-mkimage` does not check symbols.

Libraries listed in `BUNDLE_LIBS` are staged into the app's private prefix and
found through `RPATH=$ORIGIN/../lib/<name>`, which resolves correctly in both
products. For the image, `deluge-mkimage` bakes the prefix into the packed
rootfs at `usr/lib/<name>/`, so the `.ELF` is self-contained. For the bare
binary, `deluge-mkimage` checks the prefix against the rootfs but does not
deliver it; you copy it to the card alongside the binary. They land at:

```
build/.../card/LINUX/APPS/<name>        → the card's /LINUX/APPS/<name>
build/.../card/LINUX/lib/<name>/*.so    → the card's /LINUX/lib/<name>/
```

This mirrors the card's own `/LINUX` layout, so deployment is a single
`cp -r card/LINUX /sd/`. The prefix sits next to `APPS/` rather than inside it
because the card is vfat, which presents every file as mode 0755, and the
launcher treats any file under `APPS/` as an app: a `.so` there would appear in
its menu.

Staged libraries are copied as real files named by `DT_SONAME`, because vfat
cannot store the usual `.so.1 → .so.1.2.3` symlink chain. For a library target
built with `VERSION`/`SOVERSION`, pass `$<TARGET_SONAME_FILE:tgt>` to
`BUNDLE_LIBS`, not `$<TARGET_FILE:tgt>`. The latter is the versioned real file;
staging it under its own name produces a prefix that `dlopen` and `DT_NEEDED`
cannot find at run time. For an unversioned `.so` (no `SOVERSION`) the two are
the same file, so either works. See `examples/dlopen_demo/` for a working
example.

## Rust

For Rust apps, use [deluge-sdk](https://github.com/FirestormAudio/deluge-sdk)
instead of libdeluge.
