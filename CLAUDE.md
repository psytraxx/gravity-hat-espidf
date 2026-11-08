# CLAUDE.md

Project guidance for Claude Code working in this repository.

## What this is

ESP-IDF 6.1 firmware in C for a "gravity hat": a 500-LED WS2812B strip
wound in a spiral round a hat's crown, on an
ESP32-WROOM-32 with an MPU-6500 IMU and an I2S microphone. README.md
describes the hardware, wiring and design.

## Layout

- `main/core/` holds everything that needs no hardware (fluid, LED
  spiral, colours, motion filter, tap detector, sound pulse, IMU decoding). It must not include
  ESP-IDF or `sdkconfig.h` headers, so it builds on the host.
- `main/*.c` are the ESP-IDF parts: main loop, MPU-6500 and microphone drivers, WS2812
  driver. Pins and options come from `main/Kconfig.projbuild`.
- `test/host/` has Unity tests for `main/core`; every `test_*.c` there is
  one test binary.

## Before committing

```bash
cmake -S test/host -B build-host && cmake --build build-host
ctest --test-dir build-host --output-on-failure
idf.py build
```

Host tests build with `-Wall -Wextra -Werror`; keep the firmware build
free of warnings too.

## House style

- Document the status quo: README and comments describe the design as it
  is now, not how it got here. The same goes for commit messages.
- Physics constants are per step, at the step period in menuconfig.
- Mermaid for diagrams, not ASCII art.
