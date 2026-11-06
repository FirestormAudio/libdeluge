# Changelog

All notable changes to libdeluge. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). Until 1.0, minor versions may break
the API.

## [0.1.0] - 2026-11-06

First public release.

### Added

- `deluge_open()` / `deluge_close()`: one handle per device, opened
  capability by capability. A missing device leaves that capability `NULL`
  instead of failing the whole open.
- **Audio:** real-time callback thread over ALSA with `float` ⇄ S24_LE
  conversion, and recovery from xruns on the linked playback/capture pair.
  `deluge_audio_running()` reports whether the thread is still going.
- **MIDI:** DIN rawmidi read/write, plus a reader thread that pushes incoming
  messages to a callback.
- **Input:** pads (multi-touch), buttons, encoders and clock-in as one
  `deluge_event` stream, delivered on a library-owned thread or read from
  your own poll loop.
- **Display:** OLED and pad-RGB framebuffers (mmap blit), and a configurable
  pad-LED refresh interval.
- **LEDs:** indicator, gold-column and SYNC LEDs.
- **CV / gate:** CV DACs through IIO, four gate outputs through the GPIO v2
  uAPI.
- **Jacks:** jack-detect state and speaker control.
- **SRAM:** a 1 MB window of the RZ/A1L's on-chip SRAM as a cached `mmap`
  scratch area.
- **USB:** dual-role get/set, hotplug watching over uevent netlink, and
  persistent USB MIDI and USB audio handles that rebind across gadget and host
  mode.
- **CMake:** `deluge_add_app()` (static or dynamic linkage), `FetchContent`
  support, `find_package(Deluge)`, and `deluge.pc`.

### Known limitations

- USB audio is discovery and probing only; streaming is not implemented in
  0.1.x (`include/deluge/usb_audio.h`).
- USB MIDI uses rawmidi bytes; native UMP is not supported yet.
- `deluge_cv_set_volts()` uses the stock firmware's nominal scale (6552
  counts per volt, unipolar), with no per-unit calibration.

[0.1.0]: https://github.com/FirestormAudio/libdeluge/releases/tag/v0.1.0
