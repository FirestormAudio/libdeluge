# libdeluge

libdeluge is the C library for writing apps for the [Synthstrom Deluge]
running [deluge-linux]. It covers audio I/O, MIDI, pads and buttons, the OLED
and pad LEDs, CV/gate, jack detection, fast SRAM and USB, and ships the CMake
integration (`deluge_add_app()`) that packs your app into an image the Deluge
can boot.

It is a static C11 library with a C API, usable from C and C++. For Rust, use
[deluge-sdk] instead.

## Getting started

1. Download a `deluge-base-<version>.tar.zst` bundle from the
   [deluge-linux releases] and unpack it. It contains the cross toolchain,
   the base rootfs and the image packer.

   ```sh
   tar --zstd -xf deluge-base-<version>.tar.zst
   export DELUGE_BASE=$PWD/deluge-base-<version>
   ```

2. Add libdeluge to your app's `CMakeLists.txt`:

   ```cmake
   include(FetchContent)
   FetchContent_Declare(deluge
     GIT_REPOSITORY https://github.com/FirestormAudio/libdeluge.git
     GIT_TAG v0.1.0)
   FetchContent_MakeAvailable(deluge)
   deluge_add_app(myapp SOURCES main.c)
   ```

3. Build with the bundle's toolchain file:

   ```sh
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DELUGE_BASE/DelugeToolchain.cmake
   cmake --build build
   ```

   This produces `build/MYAPP.ELF` and `build/card/LINUX/APPS/myapp`.

4. Install on the SD card. Either copy `MYAPP.ELF` to `/APPS/`, or copy
   `build/card/LINUX` to the card so the bare binary lands in `/LINUX/APPS/`.

5. Launch `MYAPP.ELF` from the boot menu of the
   [app-loader](https://github.com/FirestormAudio/deluge-sdk/blob/main/docs/app-loader.md),
   which you install once like a firmware update. deluge-linux's [boot.md]
   covers installing it and uploading over USB instead.

[docs/building-an-app.md](docs/building-an-app.md) covers the full build:
the project layout, CPM, dynamic linking and plugins, and what each product
contains.

## Documentation

- [Building an app](docs/building-an-app.md): CMake integration, products,
  linkage and deployment.
- [API overview](docs/api.md): lifecycle, errors, threading and the real-time
  rules for the audio callback. Each header in
  [`include/deluge/`](include/deluge/) documents its functions.
- deluge-linux's [drivers.md] describes the kernel interfaces libdeluge is
  built on.

## Examples

| example | what it does |
| --- | --- |
| [`sine_out`](examples/sine_out.c) | Plays a 440 Hz sine for 3 seconds through the audio callback. |
| [`midi_echo`](examples/midi_echo.c) | Echoes DIN MIDI in to out for 10 seconds. |
| [`event_dump`](examples/event_dump.c) | Prints pad, button, encoder and clock events for 30 seconds. |
| [`display_demo`](examples/display_demo.c) | Draws on the OLED and pads and lights the LEDs. |
| [`cv_demo`](examples/cv_demo.c) | Ramps CV 0, pulses gate 0, and reports the SRAM mapping. |
| [`deluge-selftest`](examples/selftest.c) | Exercises every capability and prints PASS or WARN. |
| [`usb_role`](examples/usb_role.c) | Prints or switches the USB port role. |
| [`usb_data`](examples/usb_data.c) | Reports the USB MIDI and USB audio handles and probes the USB audio card. |
| [`usb_midi_dump`](examples/usb_midi_dump.c) | Prints bytes received over USB MIDI. |
| [`usb_audio_measure`](examples/usb_audio_measure.c) | Measures the USB audio clock against the codec clock. |
| [`example`](examples/app/) | The smallest `deluge_add_app()` app: blinks an LED. |
| [`dlopen_demo`](examples/dlopen_demo/) | A `LINKAGE DYNAMIC` app that `dlopen`s a bundled plugin. |

The examples build as part of a top-level build of this repository. All but
the last two are plain executables (`add_executable`), not packed images: they
also build on the host, where they compile but have no hardware to talk to.
When cross-compiled they are dynamically linked against the bundle's rootfs
libraries, so run them on an image built from the same bundle.
To run one on the Deluge:

1. Cross-compile, and copy the binary from `build/examples/` to the SD card.
2. Boot the `platform` image (`LINUX-platform.ELF` from the deluge-linux
   release, or `make platform appelf` in deluge-linux). It has a shell; the
   `base` image does not.
3. Log in as `root` (password `deluge`) over the USB serial console or
   `ssh root@192.168.7.2`. The SD card is mounted at `/sd`.
4. Run it, e.g. `/sd/midi_echo`.

`example` and `dlopen_demo` use `deluge_add_app()` and are built only when
cross-compiling; their products land under `build/examples/app/` and
`build/examples/dlopen_demo/`.

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DELUGE_BASE/DelugeToolchain.cmake
cmake --build build
```

## Building and testing libdeluge

A host build runs the test suite against fixture files, with no Deluge or
bundle needed. It needs CMake 3.21 or later, a C11 compiler and the ALSA
development headers.

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

`MIT OR Apache-2.0`. See [LICENSE-MIT](LICENSE-MIT) and
[LICENSE-APACHE](LICENSE-APACHE).

[Synthstrom Deluge]: https://synthstrom.com/product/deluge/
[deluge-linux]: https://github.com/FirestormAudio/deluge-linux
[deluge-linux releases]: https://github.com/FirestormAudio/deluge-linux/releases
[deluge-sdk]: https://github.com/FirestormAudio/deluge-sdk
[boot.md]: https://github.com/FirestormAudio/deluge-linux/blob/main/docs/boot.md
[drivers.md]: https://github.com/FirestormAudio/deluge-linux/blob/main/docs/drivers.md
