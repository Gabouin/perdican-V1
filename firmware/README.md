# PERDICAN V1 Firmware

Bare-metal firmware for the [PERDICAN V1](https://github.com/Gabouin/perdican-V1)
devboard: **STM32G431CBT6** + **LSM6DS3TR-C** IMU, with a USB CDC console on
the board's USB-C port.

Every pin assignment in this firmware was extracted from the board's own
fabrication netlist (`production/pcb/netlist.ipc`), not transcribed by eye,
so the pin map is exactly what the copper does.

```
  ____  _____ ____  ____ ___ ____    _    _   _
 |  _ \| ____|  _ \|  _ \_ _/ ___|  / \  | \ | |
 | |_) |  _| | |_) | | | | | |     / _ \ |  \| |
 |  __/| |___|  _ <| |_| | | |___ / ___ \| |\  |
 |_|   |_____|_| \_\____/___\____/_/   \_\_| \_|  V1
```

---

## What it does

- Brings SYSCLK to **170 MHz** from the internal HSI16, since the board has
  no crystal, so everything runs off internal oscillators.
- Runs USB from **HSI48 + CRS**, trimmed against the host's start-of-frame.
  This is what lets a crystal-less board do USB at all.
- Talks to the **LSM6DS3TR-C** over I2C1 at 400 kHz: accelerometer,
  gyroscope, temperature, data-ready interrupts and the datasheet self-test.
- Presents a **USB CDC serial console** with an interactive shell. The board
  has no USB-UART bridge, so native USB is the only way to reach a host.
- Reflashes over the same USB-C cable with **no debugger**, via DFU.

Footprint: **~31 KB flash (24%)**, **~7.6 KB RAM (35%)**.

---

## Quick start

```sh
brew install --cask gcc-arm-embedded    # or your distro's arm-none-eabi-gcc
brew install dfu-util                   # for debugger-free flashing

make                                    # builds build/perdican.{elf,bin,hex}
```

Flash it: **hold BOOT, tap RESET, release BOOT**, then:

```sh
make dfu
```

Open the console (the board appears as a USB serial port):

```sh
screen /dev/cu.usbmodem*        # macOS
screen /dev/ttyACM0             # Linux
```

Once firmware is running, later reflashes need no button at all:

```sh
make dfu-reset dfu
```

`dfu-reset` opens the port at 1200 baud and drops DTR; the firmware takes
that as a request to reboot into the ROM bootloader.

---

## The console

```
perdican> help
  help                 this list
  id                   board, clock and chip identity
  uptime               milliseconds since boot

  led on|off|toggle    drive the user LED (PA3)
  btn                  read the user button (PA2)
  vbus                 USB bus power present (PA9)

  imu                  one accel/gyro/temp reading
  imu stream [hz]      stream readings, any key stops
  imu selftest         run the datasheet self-test
  imu regs             dump the key control registers

  i2cscan              probe every address on I2C1

  pins                 expansion header map
  gpio <pin> read      read a header pin (e.g. gpio PB0 read)
  gpio <pin> out 0|1   drive a header pin
  gpio <pin> in [pu|pd]  make a header pin an input

  reset                reboot the board
  dfu                  reboot into the USB DFU bootloader
```

`gpio PA13`/`gpio PA14` refuse to reconfigure without a trailing `force`,
because those pins are SWDIO/SWCLK as well as J2-3/J2-4.

---

## Board pin map

Derived from `netlist.ipc`. Nets with only one pad were treated as
unconnected and are listed as such.

### On-board peripherals

| Function        | Pin        | Detail                                              |
|-----------------|------------|-----------------------------------------------------|
| User LED        | **PA3**    | R10 (10k) in series, cathode to GND → **active high** |
| User button     | **PA2**    | R11 (10k) pull-up to 3V3, switch to GND → **active low** |
| IMU SCL         | **PB6**    | I2C1_SCL, AF4, R7 (4.7k) pull-up                     |
| IMU SDA         | **PB7**    | I2C1_SDA, AF4, R6 (4.7k) pull-up                     |
| IMU INT1        | **PA0**    | LSM6DS3TR-C pad 4                                    |
| IMU INT2        | **PA1**    | LSM6DS3TR-C pad 9                                    |
| USB D−          | **PA11**   | USB-C A7/B7                                          |
| USB D+          | **PA12**   | USB-C A6/B6                                          |
| SWDIO           | **PA13**   | DEBUG-2, also J2-3                                   |
| SWCLK           | **PA14**   | DEBUG-3, also J2-4                                   |
| VBUS sense      | **PA9**    | R4/R5 100k divider, 5 V → 2.5 V                      |
| BOOT0           | **PB8**    | R3 (10k) to GND, BOOT button to 3V3                  |
| NRST            | pin 7      | RESET button + C9 (100 nF)                           |

The IMU is strapped for **I2C** (CS tied to 3V3) with **SA0 grounded**, so
its 7-bit address is **0x6A**.

### Expansion headers (14 GPIO)

```
      J1 (left)                     J2 (right)
   1  GND                        1  GND
   2  +3V3                       2  +3V3
   3  PB10                       3  PA13  (also SWDIO)
   4  PB2                        4  PA14  (also SWCLK)
   5  PB1                        5  PA15
   6  PB0                        6  PB3
   7  PA7                        7  PB4
   8  PA6                        8  PB5
   9  PA5                        9  PB9
```

DEBUG header: `1 = +3V3`, `2 = SWDIO`, `3 = SWCLK`, `4 = GND`.

### Pins that go nowhere

`PA4, PA8, PA10, PB11, PB12, PB13, PB14, PB15, PC13, PC14, PC15, PF0, PF1`
have no copper connection on this board. The firmware parks them in analog
mode, which is ST's recommended lowest-leakage state for unused I/O.

---

## Two hardware notes

**VBAT is not tied to VDD.** Pad 1 has only C10 (100 nF) to +3V3, so it is
AC-coupled and DC-floating. ST requires VBAT be strapped to VDD when no
backup cell is fitted, so the backup domain on this board is out of spec.
**Do not use the RTC, LSE or TAMP backup registers.** This firmware keeps its
reset-persistent state (the DFU magic word) in a `.noinit` SRAM word instead.
Worth fixing in V2: a short from VBAT to 3V3.

**VBUS sense is digital only.** The 100k/100k divider puts 2.5 V on PA9,
above the 2.31 V input-high threshold, so it reads as a clean "USB power
present" flag. PA9 is not an ADC input on STM32G4, so it cannot measure the
actual bus voltage. Margin is modest; a 100k/56k divider would be more
comfortable in V2.

---

## Layout

```
include/          board.h (the pin map), plus driver headers
src/
  startup.c       vector table, reset entry, fault handler
  clock.c         170 MHz PLL, HSI48 + CRS for USB
  board.c         LED, debounced button, VBUS, unused-pin parking
  systick.c       1 kHz tick, millis/micros/delay
  i2c.c           blocking I2C1 master
  lsm6ds3.c       IMU driver incl. self-test
  dfu.c           reboot into the ROM bootloader
  console.c       the command shell
  retarget.c      printf -> USB CDC, plus newlib syscalls
  main.c          bring-up and main loop
  usb/
    usb_device.c  USB device core and control transfers
    usb_desc.c    descriptors
    usb_cdc.c     CDC ACM class, ring buffers
linker/           STM32G431CBT6 memory layout
cmsis/            vendored CMSIS-Core + ST device header
tools/            dfu-touch.sh, openocd.cfg
```

There is no HAL. Drivers are written directly against the register file, so
each one is short enough to read end to end.

---

## Flashing over SWD

The DEBUG header is SWD-only (no NRST pin), so a hard reset means pressing
the RESET button.

```sh
make flash                                  # OpenOCD + ST-Link
openocd -f tools/openocd.cfg                # then: make gdb
```

---

## Details worth knowing if you extend this

- **I2C timing is derived, not generated.** I2C1 is clocked from HSI16 so
  `TIMINGR` stays valid regardless of SYSCLK; the arithmetic behind the two
  constants is written out in `src/i2c.c`.
- **USB PMA is linear on STM32G4** (`PMA_ACCESS = 1`), unlike STM32F1 where
  each 16-bit entry sits at a 32-bit stride. `src/usb/usb_device.c` depends
  on this.
- **The accelerometer full-scale encoding is not in ascending order**
  (`00 = ±2 g`, `01 = ±16 g`, `10 = ±4 g`, `11 = ±8 g`). Getting it wrong
  scales every reading by 8× and looks like a calibration problem.
- **170 MHz needs Range 1 boost mode**, and its entry sequence requires
  stepping through HCLK/2 for at least 1 µs. `src/clock.c` does this.

---

## License

MIT, matching the hardware project. Vendored CMSIS headers keep their
original Apache-2.0 (ARM) and BSD-3-Clause (ST) licenses.
