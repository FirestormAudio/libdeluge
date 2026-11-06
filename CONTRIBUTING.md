# Contributing to libdeluge

## Build and test

The host build needs CMake ≥ 3.21, a C11 compiler and the ALSA development
headers (`libasound2-dev` on Debian/Ubuntu). It runs the full test suite against
fixture files, with no Deluge attached:

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

To cross-compile for the Deluge, point `DELUGE_BASE` at an unpacked
[deluge-linux] release bundle and use its toolchain file (see
[docs/building-an-app.md](docs/building-an-app.md#get-the-bundle)):

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DELUGE_BASE/DelugeToolchain.cmake
cmake --build build
```

To build deluge-linux against your local checkout instead of the published tag:

```sh
make base LIBDELUGE_OVERRIDE_SRCDIR=/path/to/libdeluge   # in deluge-linux
```

## Guidelines

- The build uses `-Wall -Wextra -Werror`, so a warning is a build failure.
- Every source file starts with an SPDX line:
  `/* SPDX-License-Identifier: MIT OR Apache-2.0 */`.
- Document every public function in `include/deluge/` with a Doxygen block:
  `@brief`, `@param` for each argument, and `@return` listing the success
  value and each `deluge_err` it can return, including the NULL-handle case.
  Note ownership of returned pointers and descriptors, and which thread a
  callback runs on. Use `@note` / `@warning` rather than capitals for emphasis.
- Keep [docs/api.md](docs/api.md) in step with the headers when a change
  affects the lifecycle, error, threading or real-time conventions.
- Logic that doesn't need hardware (decoders, parsers, sysfs/procfs readers)
  gets a host test in `tests/`, with fixtures under `tests/fixtures/`.
- libdeluge is written against the kernel interfaces documented in
  deluge-linux's [docs/drivers.md]. If a change needs a new or different
  interface, change it there first.
- Commit messages use [Conventional Commits](https://www.conventionalcommits.org/)
  (`feat(usb): …`, `fix(audio): …`).
- Add user-visible changes to [CHANGELOG.md](CHANGELOG.md).

## License

By contributing, you agree that your contributions are dual-licensed under
`MIT OR Apache-2.0`, as described in the README.

[deluge-linux]: https://github.com/FirestormAudio/deluge-linux
[docs/drivers.md]: https://github.com/FirestormAudio/deluge-linux/blob/main/docs/drivers.md
