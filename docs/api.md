# API overview

This page covers the conventions shared by every libdeluge header. The headers
in [`include/deluge/`](../include/deluge/) document each function: its
arguments, return values, and which errors it can return.

## Lifecycle

```c
#include <deluge/deluge.h>
#include <deluge/leds.h>

deluge_t *d = deluge_open();
if (!d) return 1;

deluge_leds_t *leds = deluge_leds(d);   /* NULL if the LEDs are absent */
if (leds) deluge_leds_indicator(leds, 0, 1);

deluge_close(d);
```

- `deluge_open()` opens every capability it can find. It returns NULL only if
  allocating the handle fails.
- Each capability has an accessor (`deluge_audio()`, `deluge_midi()`,
  `deluge_input()`, ...) that returns a handle owned by the `deluge_t`. Do not
  free it. It stays valid until `deluge_close()`, and calling the accessor
  again returns the same handle.
- If a device is missing, its accessor returns NULL (some capabilities also
  print the reason on stderr). The rest of the handle still works, so an app can check
  each capability and degrade gracefully.
- Three handles exist even when their hardware is absent:
  `deluge_audio()` (the codec is opened by `deluge_audio_start()`), and
  `deluge_usb_midi()` and `deluge_usb_audio()`, which stay valid across USB
  role switches and hotplug. Their functions return `DELUGE_ERR_NODEV` while
  nothing is bound; call the matching `_rebind()` function after a role switch
  or hotplug event.
- `deluge_close()` stops every library-owned thread, closes every descriptor,
  unmaps every mapping, and frees the handle. Pointers returned by the `_map()`
  functions and descriptors returned by the `_fd()` functions become invalid.

## NULL handles

Every function accepts a NULL capability handle, so you can pass an
accessor's result straight through. Functions that return `int` return a
negative error (usually `DELUGE_ERR_STATE`, `DELUGE_ERR_INVAL` for the
`_start()` and `_watch()` functions). Functions that return a pointer return
NULL, `_fd()` functions return -1, `_stop()` functions do nothing, and
`deluge_audio_xruns()` and `deluge_audio_running()` return 0.

## Errors

Functions that return `int` return `DELUGE_OK` (0) or a non-negative count on
success, and a negative `deluge_err` from [`error.h`](../include/deluge/error.h)
on failure. `deluge_strerror()` turns any code into a short string:

```c
int rc = deluge_audio_start(deluge_audio(d), cb, NULL);
if (rc < 0) fprintf(stderr, "audio: %s\n", deluge_strerror(rc));
```

## Threads

libdeluge starts threads only when you ask for them:

| started by | priority | calls |
| --- | --- | --- |
| `deluge_audio_start()` | `SCHED_FIFO` 80 (normal if refused) | the audio callback, once per 128-frame period |
| `deluge_input_start()` | normal | the input callback, once per event |
| `deluge_midi_start()` | normal | the MIDI callback, once per read of received bytes |
| `deluge_usb_watch()` | normal | the hotplug callback, once per device node added or removed |

Each matching `_stop()` (or `deluge_usb_unwatch()`) wakes the thread and waits
for it to exit; the callback is never called after it returns.
`deluge_close()` stops all of them. Start and stop each thread from one
controlling thread, and never from inside its own callback.

Input, DIN MIDI and USB hotplug each offer two delivery models; use one, not
both at once:

- Callback: the `_start()` / `_watch()` function above.
- Your own loop: `deluge_input_fd()` with `deluge_input_read()`,
  `deluge_midi_read()` polled, or `deluge_usb_fd()` read directly.

Callbacks run on libdeluge's threads, so any state they share with the rest of
your app needs its own synchronisation. Keep the input, MIDI and hotplug
callbacks short: a slow callback delays the next event.

The other functions are plain synchronous calls with no internal locking. Do
not call functions on the same handle from two threads at once.

## Real-time rules for the audio callback

The audio callback runs on a real-time thread, once per period of
`DELUGE_PERIOD` (128) frames at `DELUGE_SR` (44100 Hz): about 2.9 ms. If it
takes too long, the codec underruns and `deluge_audio_xruns()` counts it.
Inside the callback:

- Do not allocate or free memory.
- Do not block: no mutexes shared with other threads, no sleeping, no waiting
  on I/O.
- Do not make system calls. That includes `printf`, file I/O, and almost every
  libdeluge function; `deluge_audio_xruns()`, `deluge_audio_running()` and
  `deluge_strerror()` are the exceptions.
- Write every output sample; the buffer still holds the previous period.

Pass data to and from the callback through preallocated, lock-free structures
such as a single-producer, single-consumer ring buffer.

The thread asks for `SCHED_FIFO`. That needs `CAP_SYS_NICE` or a suitable
`RLIMIT_RTPRIO`; without it libdeluge prints a message and runs the thread at
normal priority, where xruns are more likely.

## Headers

| header | covers |
| --- | --- |
| [`deluge.h`](../include/deluge/deluge.h) | `deluge_open()` / `deluge_close()` |
| [`error.h`](../include/deluge/error.h) | `deluge_err` codes and `deluge_strerror()` |
| [`audio.h`](../include/deluge/audio.h) | Internal codec: real-time callback with interleaved stereo `float` I/O |
| [`midi.h`](../include/deluge/midi.h) | 5-pin DIN MIDI as raw bytes: polled reads or a reader thread |
| [`input.h`](../include/deluge/input.h) | Pads, buttons, encoders and clock input as one `deluge_event` stream |
| [`display.h`](../include/deluge/display.h) | OLED and pad-LED framebuffers, and the pad-LED refresh interval |
| [`leds.h`](../include/deluge/leds.h) | Indicator, gold-knob and SYNC LEDs |
| [`cv.h`](../include/deluge/cv.h) | Two CV outputs and four gate outputs |
| [`jacks.h`](../include/deluge/jacks.h) | Jack-detect state and the speaker switch |
| [`sram.h`](../include/deluge/sram.h) | 1 MB of fast on-chip SRAM as cached scratch memory |
| [`usb.h`](../include/deluge/usb.h) | USB port role and hotplug notification |
| [`usb_midi.h`](../include/deluge/usb_midi.h) | USB MIDI as raw MIDI 1.0 bytes |
| [`usb_audio.h`](../include/deluge/usb_audio.h) | USB audio card discovery and probing (no streaming yet) |

## Button ids

`DELUGE_EV_BUTTON` events carry the front-panel controller's button id. For a
button with an indicator LED, the same number is its id for
`deluge_leds_indicator()`. The names follow deluge-sdk's `controls::button`.

| id | button | id | button |
| --- | --- | --- | --- |
| 0 | scroll Y encoder (push) | 18 | upper gold knob (push) |
| 1 | encoder function 0 | 19 | encoder function 2 |
| 2 | encoder function 4 | 20 | encoder function 6 |
| 3 | affect entire | 21 | clip view |
| 5 | synth | 23 | MIDI |
| 6 | scale mode | 24 | cross-screen edit |
| 7 | learn | 25 | sync scaling |
| 8 | shift | 26 | record |
| 9 | scroll X encoder (push) | 27 | lower gold knob (push) |
| 10 | encoder function 1 | 28 | encoder function 3 |
| 11 | encoder function 5 | 29 | encoder function 7 |
| 12 | session view | 30 | keyboard |
| 13 | tempo encoder (push) | 31 | select encoder (push) |
| 14 | kit | 32 | CV |
| 15 | load | 33 | save |
| 16 | back | 34 | tap tempo |
| 17 | triplets | 35 | play |

Ids 4 and 22 have no name in that list.
