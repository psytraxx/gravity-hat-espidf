# Gravity hat (ESP-IDF)

Firmware for a hat of light: a WS2812B strip wound in a spiral round a
hat's crown shows a liquid inside it. Tilt the hat and the surface
tilts; shake it and the fluid sloshes; lay it on its
side and the fluid runs along the low side. Its brightness pulses with
the sound around it, and a double tap gives the fluid a new, random
colour.

It runs on an ESP32-WROOM-32 with an MPU-6500 IMU and an I2S
microphone, and is written in C for ESP-IDF 6.1. The physics is a port
of the fluid solver in [gravity-cube-rs](https://github.com/psytraxx/gravity-cube-rs).

## Hardware

| Part | Notes |
|---|---|
| ESP32-WROOM-32 board | 4 MB flash |
| MPU-92/65 board | MPU-6500 (chip marked MP65), 3–5 V supply (on-board regulator), wired for SPI |
| WS2812B strip, 500 LEDs | Wound clockwise (seen from above) in a spiral of about 13 turns from the bottom up: 39 LEDs a turn at the bottom, 36.8 at the top, as the crown narrows |
| I2S microphone (e.g. INMP441) | On GPIO 32, 33 and 25; its sound pulses the brightness |
| 330 Ω resistor | In series with the data line, at the strip's input |
| 1000 µF capacitor or more | Across 5 V and GND at the strip's input (10 V or more) |
| 1N5819 Schottky diode | From the 5 V supply to the board's 5V pin, so USB cannot feed the strip or the supply |
| 5 V supply | Sized for the LEDs; the firmware keeps them dim (one channel at 60/255 or less) |

## Wiring

![Wiring](docs/wiring.svg)

On the board's pins (a 38-pin ESP32-WROOM-32 board, its pins as printed
in its pinout picture):

![Pinout](docs/pinout.svg)

### Power

The 5 V supply feeds the strip directly and the board through the
1N5819, anode on the supply, cathode (the band) on the board's 5V pin.
As a Schottky it drops about 0.3 V. With USB plugged in as well, the
diode keeps USB's 5 V out of the supply and the strip; the board then
runs from whichever is higher. The capacitor sits across 5 V and GND
where the supply meets the strip, + to 5 V. All grounds are common.

### LEDs

| ESP32 | Goes to |
|---|---|
| GPIO 13 | 330 Ω → DIN of the strip |

GPIO 13 sits next to GND and near 5V on the board's header, and has no
role at boot.

The ESP32 drives the data line at 3.3 V; WS2812B LEDs want about 3.5 V
at a 5 V supply. Most take 3.3 V; if the first LEDs flicker or show
wrong colours, add a 74AHCT125 level shifter.

### MPU-6500 (SPI)

The IMU uses the ESP32's VSPI pins, together on one side of the board:

| MPU-92/65 board | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL (SCLK) | GPIO 18 (VSPI CLK) |
| SDA (SDI / MOSI) | GPIO 23 (VSPI MOSI) |
| AD0 (SDO / MISO) | GPIO 19 (VSPI MISO) |
| NCS | GPIO 5 (VSPI CS) |
| FSYNC | GND |
| EDA, ECL, INT | not connected |

FSYNC is unused and tied to GND so it does not float. EDA and ECL are an
auxiliary I2C bus for a magnetometer, and INT an interrupt line; the
firmware polls the FIFO instead.

SPI runs at 1 MHz, against 400 kHz for the board's I2C interface, so a
frame's FIFO read takes a fraction of the time. The pins are settable in
menuconfig. GPIO 5 is a strapping pin that has to be high at boot; NCS
idles high, so it is.

### I2S microphone

| Microphone board | ESP32 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SD | GPIO 32 (I2S data) |
| SCK | GPIO 33 (I2S bit clock) |
| WS | GPIO 25 (I2S word select) |
| L/R | GND (left channel) |

The three pins sit next to each other on the header, and none has a
role at boot; they are settable in menuconfig. The data line has the
ESP32's pull-down on, so with no microphone it reads silence and the
firmware keeps the fluid at full brightness. In WLED's
sound-reactive build the same wiring is `"digitalmic": {"type": 1,
"pin": [32, 25, 33, -1]}` (SD, WS, SCK, no MCLK), with the LEDs on
GPIO 13.

### The strip's geometry

The firmware places every LED from a few numbers in
[`main/core/config.h`](main/core/config.h); [Setting up a
hat](#setting-up-a-hat) shows how to read them off. For this hat:

| Setting | Value | |
|---|---|---|
| `HAT_LEDS_PER_TURN_BOTTOM` | 39 | LEDs in the bottom turn |
| `HAT_LEDS_PER_TURN_TOP` | 36.8 | LEDs in the top turn; fewer than at the bottom where the hat narrows |
| `HAT_ROW_PITCH` | 0.65 | Distance between two turns, divided by the distance between two LEDs along the strip |
| `HAT_LED_SPACING_MM` | 10 | Distance between two LEDs along the strip; sets how fast the fluid moves |
| `HAT_START_AT_TOP` | 0 | 1 if the data enters at the top turn |
| `HAT_CLOCKWISE` | 1 | 1 if the strip winds clockwise seen from above |

The LEDs per turn change evenly from bottom to top, so a hat that
narrows works too; from 39 down to 36.8 a turn, 500 LEDs make about
13 turns. The hat's radius at each height follows from its LEDs per turn
there. The grid round the hat (`FLUID_NX`, `FLUID_NY`, `FLUID_NZ`) has
to hold it: the host tests check, and fail if a new geometry needs a
bigger grid.

The IMU board lies flat in the hat's crown, chip side up, its X
arrow pointing any way round: *IMU rotation* in menuconfig says which,
and the firmware turns its readings into the hat's axes. Mounted on its
side or upside down, edit `MPU6500_AXIS_MAP` in
[`main/core/mpu6500_decode.h`](main/core/mpu6500_decode.h).

## Build and flash

ESP-IDF 6.1 ([install guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/get-started/)):

```bash
idf.py build
```

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Keep the hat still for the first second after power on: the gyro
calibrates its zero offset then.

The monitor logs a status line every 100 frames: acceleration,
rotation rate, sound level and brightness, and the physics, frame and
LED send times.

## Setting up a hat

The firmware has to know where every LED sits and how the IMU lies. For
a new hat, or after rewinding the strip:

1. **Count the strip.** Note:
   - the turn the data enters, bottom or top: `HAT_START_AT_TOP`;
   - which way the strip winds from there, seen from above:
     `HAT_CLOCKWISE`;
   - the LEDs in the bottom turn and in the top turn, counted round the
     hat: `HAT_LEDS_PER_TURN_BOTTOM`, `HAT_LEDS_PER_TURN_TOP`;
   - the distance between two rows over the distance between two LEDs
     along the strip: `HAT_ROW_PITCH`;
   - 1000 over the strip's LEDs per metre: `HAT_LED_SPACING_MM`.

   Set them in [`main/core/config.h`](main/core/config.h) and run the
   host tests (they fail if the grid needs to grow).
2. **Calibration picture on.** In `idf.py menuconfig` → *Gravity hat*,
   turn on *Calibration picture instead of the fluid*, then build and
   flash. Updates over Wi-Fi keep working, so the steps below can be
   flashed over the network.
3. **Straighten the lines.** Four lines run up the hat, one LED per
   turn: red at LED 0's angle, then green, blue and yellow a quarter turn
   apart, counter-clockwise seen from above. Where the LEDs per turn are
   not a whole number they step half an LED from side to side; past that
   they should stand straight. Follow a line from LED 0's end: where it
   drifts the way the strip runs, the LEDs per turn set there are too
   many; where it drifts against it, too few. Drift that grows only
   towards the far end means the far turns are shorter or longer than
   the near ones: change the count at that end. Over 13 turns, 0.1 LED
   more a turn at one end moves the far end of the line about 0.65 LEDs,
   at both ends about 1.3. Counting by hand is seldom exact; a few rounds
   of looking at the lines settle it.
4. **Mount the IMU and set its rotation.** Lay the board flat in the
   hat's crown, chip side up, its X arrow pointing any way round.
   Tilt the hat and a white band lights the side the firmware takes for
   low. With *IMU rotation* set to 0, tip the hat so that LED 0's side
   (the red line) is lowest: the band lights the side the setting points at,
   a quarter turn per line, counter-clockwise seen from above: on the
   green line 90, on the blue 180, on the yellow 270, in between in
   between. The monitor logs it exactly as `low side at N deg` (add 360
   if N is negative). Set *IMU rotation* to it, flash, and the band
   follows every tilt: red side down lights the red line, green side
   down the green. If the band moves the right way for one tilt but the
   wrong way for the other, the board is mirrored, which a right-way-up
   board cannot be: check it lies chip side up.
5. **Calibration picture off.** Build and flash: the fluid.

This hat came out at LED 0 on the brim, winding clockwise, 39 LEDs a
turn at the brim and 36.8 at the top (straightened with the lines), rows
0.65 LED spacings apart, and an IMU rotation of 90°, the default.

## Updates over Wi-Fi

Once the hat has been flashed over USB with a Wi-Fi SSID set, it takes
new firmware over the network. Set the SSID and password (and, ideally,
an update token) in `idf.py menuconfig` → *Gravity hat* → *Wi-Fi and
firmware updates*, flash over USB once, then for every update:

```bash
idf.py build
```

```bash
curl -H "Authorization: Bearer $TOKEN" \
     --data-binary @build/gravity-hat.bin http://hat.local/ota
```

`curl http://hat.local/` shows the running version (from `git
describe`) and app slot. If `hat.local` does not resolve, use the IP
address the monitor logs once Wi-Fi is up.

```mermaid
sequenceDiagram
    participant PC
    participant Hat
    participant Boot as Bootloader
    PC->>Hat: POST /ota (app image)
    Hat->>Hat: write image to the other slot, check it
    Hat-->>PC: Update written, restarting
    Hat->>Boot: restart
    Boot->>Hat: boot new image (pending)
    Hat->>Hat: Wi-Fi up and 100 frames run within 2 min?
    alt yes
        Hat->>Boot: keep new image
    else no, or it crashes first
        Boot->>Hat: boot previous image
    end
```

The 4 MB flash has two 1984 KB app slots ([`partitions.csv`](partitions.csv)):
the running firmware writes the new image into the other one and boots
it. A new image only stays if it gets onto the network and runs the main
loop for 100 frames within two minutes; if not, or if it crashes first,
the bootloader goes back to the previous image. So a broken build cannot
cut the hat off from further updates, but one built with no SSID turns
updates off for good, until the next USB flash.

Wi-Fi, the network stack and the update server run on core 1, next to
the LED task, so the physics on core 0 keeps its time. Flash writes
pause both cores for moments at a time, so the fluid stutters for the
few seconds an update takes.

Without a token, anyone on the network hat reflash the hat.

## Settings

`idf.py menuconfig` → *Gravity hat*:

| Setting | Default | |
|---|---|---|
| LED data GPIO | 13 | |
| Physics step period | 18 ms | The physics constants are per step |
| Calibration picture | off | Instead of the fluid: lines for the geometry, a band for the IMU; see [Setting up a hat](#setting-up-a-hat) |
| Tap threshold | 1000 mg | Raise it if sloshing changes the colour, lower it if taps are missed |
| IMU SPI pins | 23, 19, 18, 5 | MOSI, MISO, SCLK, CS |
| IMU rotation | 90° | Where LED 0's side lies seen from the IMU board's X arrow, counter-clockwise; see [Setting up a hat](#setting-up-a-hat) |
| I2C scan at startup | off | Wiring check: probes the IMU pins as I2C; finds the MPU-6500 at 0x68/0x69 after a power on |
| Pulse the brightness with the sound | on | Reads the I2S microphone |
| Microphone pins | 32, 33, 25 | SD, SCK, WS |
| Brightness in the quiet | 25 % | Loud sounds pulse it up to full |
| Wi-Fi SSID, password | empty | Network for updates over Wi-Fi; an empty SSID turns Wi-Fi off |
| Hostname | hat | Answers at `http://hat.local/` |
| Update token | empty | Updates need `Authorization: Bearer <token>`; empty lets anyone on the network update |

The strip's geometry and the fluid's colour are in
[`main/core/config.h`](main/core/config.h).

## How it works

```mermaid
flowchart LR
    IMU[MPU-6500 FIFO<br/>250 Hz] --> TAP[tap detector]
    IMU --> MF[motion filter]
    TAP -->|double tap: random hue| FB
    MIC[I2S microphone<br/>core 1, 16 kHz] -->|loudness| PULSE[pulse tracker]
    PULSE -->|brightness| FB
    MF -->|gravity + amplified motion| FX[fluid step]
    IMU -->|gyro, integrated| FX
    FX --> FB[frame builder:<br/>density behind each LED]
    FB -->|queue| LED[LED task<br/>core 1, led_strip]
```

**Core 0** reads every IMU sample since the last frame, runs one physics
step and builds the frame. **Core 1** sends frames to the strip with
Espressif's [led_strip](https://components.espressif.com/components/espressif/led_strip)
component over RMT (~15 ms for 500 LEDs); the two overlap, and core 0
only waits if core 1 is still sending. Unchanged frames are not re-sent,
so a settled hat in the quiet, or with no microphone, keeps the data
line quiet.

**IMU.** The MPU-6500 samples accelerometer (±4 g) and gyro (±1000 °/s)
at 250 Hz into its FIFO. Each frame uses the mean acceleration and the
gyro integrated over all samples since the last frame.

**Motion filter.** A complementary filter tracks gravity: the gyro turns
the estimate as the hat tilts, and the accelerometer slowly corrects it.
Whatever acceleration is left over is the hat's own motion, and is
tripled before reaching the fluid, so shaking sloshes it but tilting does
not overshoot.

**Double tap.** The MPU-6500 has no tap engine outside its DMP, so taps
are found in software on the raw samples: a jump of more than the
threshold between two samples, after at least 30 ms of stillness, counts
as a tap; a second tap within 375 ms picks a random hue at least 60°
from the current one. Shaking never has the stillness, so it does not
change the colour. Every hue's channels add up to 60/255, so the strip
draws the same current in any colour.

**Sound.** A task on core 1 reads the microphone at 16 kHz (24-bit
samples in 32-bit slots, both slots, so the L/R pin may go either way)
and keeps the RMS of the latest 20 ms, its DC removed. Each frame turns
that into a level in dB and places it between two followers: the floor,
the quiet between sounds (it drops at once to a quieter level and creeps
up 3 dB a second through louder ones), and the peak, the loud parts (it
jumps at once and sinks 6 dB a second, staying at least 12 dB above the
floor). A level 3 dB over the floor starts to count; at the peak it
counts in full. The pulse jumps to that share at once and fades with a
150 ms time constant, and the brightness runs from the quiet setting
(25 %) at no pulse to full at a full one. So a clap in a quiet room
flashes the fluid, steady noise settles back to dim within seconds, and
beats over loud music still pulse.

**Fluid.** An incompressible FLIP fluid: inviscid Navier-Stokes on a
staggered grid of 9 × 9 × 7 cells, each 1.4 LED spacings across, about
2 rows high (coarser cells would look blockier, finer ones cost too
much time on an ESP32). The hat is the cells whose centres lie within
its radius at their height and below its top: 7 layers of 61 cells,
narrowing to 57 at the top, as round as cells that size allow; its
walls are their outer faces. Faces with no cell of the hat on either
side take no part, as faces beyond the grid's edge do. Each step the
particles take gravity and the forces of the turning hat (Euler,
Coriolis, centrifugal), hand their velocity to the grid, and a
Gauss-Seidel pressure solve (60 passes) makes the flow divergence-free
against the walls. The particles take back the change (80 % FLIP, 20 %
PIC to damp noise), move, and are pushed apart so they stay evenly
spread; cells packed above two particles get a little outflow, so the
fluid keeps its volume. About 370 particles fill the bottom half.

The fluid's hat is closed and narrow, so shoving it sideways only
presses on the fluid: it moves where it has a free surface to move into. Spun about
its axis, the fluid mostly stays put while the hat turns round it, as
liquid in a glass does at first.

**LEDs.** Each LED's place on the spiral follows from its index: the
LEDs per turn change evenly from 39 at the bottom to 36.8 at the top, so
LED *i* sits where the LEDs counted so far reach *i*, clockwise from
LED 0 and climbing a row a turn, on the circle of the hat's radius at
that height. It shows the fluid half a cell inside the wall in front of
it, blended from the nearest cell centres: it lights once the density
there reaches 1.0 and goes dark below 0.4, so a still surface does not
flicker. Because the spiral climbs a little
with every LED, a tilted surface shows as a smooth slanted line rather
than steps.

## Code layout

| Path | |
|---|---|
| `main/main.c` | Main loop on core 0, LED task on core 1, calibration picture |
| `main/ota.c` | Wi-Fi, mDNS and the update server on core 1; keeps or rolls back a new image |
| `main/mpu6500.c` | MPU-6500 over SPI: setup, gyro calibration, FIFO reads |
| `main/mic.c` | I2S microphone on core 1: the loudness of the latest 20 ms |
| `main/ws2812.c` | The strip through the `espressif/led_strip` component (RMT) |
| `main/core/` | Everything that needs no hardware: fluid, LED spiral and calibration picture, colours, motion filter, tap detector, sound pulse, IMU sample decoding |
| `test/host/` | Unity tests for `main/core`, run on the host |

## Tests

The core has host tests (plain CMake and Unity, no ESP-IDF):

```bash
cmake -S test/host -B build-host && cmake --build build-host
```

```bash
ctest --test-dir build-host --output-on-failure
```

CI runs them and builds the firmware for the ESP32 on every push and
pull request.
