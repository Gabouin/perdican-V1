# PERDICAN V1 — hardware notes from writing the firmware

Everything here was derived from the board's own fabrication netlist,
`production/pcb/netlist.ipc`, by counting how many pads sit on each net. A
net with exactly one pad has no copper connection to anything else, however
it may be labelled in the schematic.

---

## Two things worth changing in V2

### 1. VBAT is left floating

`STM32 pad 1 (VBAT)` sits on a net with **one other pad only**: `C10-2`.
`C10-1` goes to `+3V3`. So VBAT is AC-coupled to the 3.3 V rail through a
100 nF capacitor and has **no DC path anywhere**.

ST requires VBAT to be tied to VDD whenever no backup cell is fitted. With
it floating:

- the backup domain has no defined supply,
- the RTC, LSE and TAMP backup registers are unreliable,
- quiescent current can be higher than the datasheet figure.

The core itself runs fine, which is why this is easy to miss.

**Firmware consequence:** this firmware never touches the backup domain. The
DFU reboot flag, which has to survive a warm reset, lives in a `.noinit`
SRAM word instead of a TAMP backup register.

**Fix for V2:** a direct short (or 0 Ω) from VBAT to +3V3. C10 can stay as
the VBAT decoupling cap it was presumably meant to be.

### 2. VBUS sense has little margin

`VBUS —[R4 100k]— PA9 —[R5 100k]— GND`

At 5.0 V VBUS this puts **2.50 V** on PA9. The STM32G4 input-high threshold
is 0.7 × VDD = **2.31 V**, so it reads high — but only by 190 mV, before
accounting for VBUS sagging under load or resistor tolerance.

PA9 is **not an ADC-capable pin** on STM32G4, so the divider cannot be used
to measure bus voltage; it can only ever be a digital present/absent flag.

**Fix for V2:** either a lower ratio (100k/56k gives ~1.8 V, comfortably
below V_IL for "absent" logic if inverted, or move to a divider that lands
near 3.0 V), or route VBUS sense to an ADC-capable pin such as PA4 or PB0 so
the voltage can actually be read.

---

## Confirmed strapping

| Item | Finding | Consequence |
|------|---------|-------------|
| IMU CS (pad 12) | tied to `+3V3` | LSM6DS3TR-C is in **I2C** mode, not SPI |
| IMU SDO/SA0 (pad 1) | tied to `GND` | 7-bit address is **0x6A** |
| IMU SDx/SCx (pads 2, 3) | tied to `GND` | auxiliary/sensor-hub bus unused |
| USB CC1/CC2 | 5.1k to GND (R1, R2) | UFP/device, 500 mA default, no PD |
| LED PA3 | R10 in series, cathode to GND | **active high** |
| Button PA2 | R11 10k to 3V3, switch to GND | **active low**, external pull-up |
| BOOT0 PB8 | R3 10k to GND, button to 3V3 | idles low, usable as GPIO at run time |
| PF0/PF1, PC14/PC15 | unconnected | **no crystal** — internal oscillators only |
| D+ pull-up | none fitted | correct: the G431 has an internal one (`USB_BCDR.DPPU`) |

The missing crystal is why the firmware runs USB from **HSI48 + CRS**,
trimmed against the host's start-of-frame packets. Without CRS, HSI48's ~3%
error is far outside USB's 0.25% requirement and the device would not
enumerate reliably.

---

## Pins with no connection

Exactly one pad on their net, i.e. routed nowhere:

```
PA4   PA8   PA10
PB11  PB12  PB13  PB14  PB15
PC13  PC14  PC15
PF0   PF1
```

These are labelled in the schematic but do not reach a header or a
component. The firmware parks them in analog mode, ST's recommended
lowest-leakage state for unused I/O.

Note that this leaves several genuinely useful peripherals stranded —
notably `PB11`–`PB14` (which include ADC and SPI2 pins). If V2 has header
room, breaking out even two of them would widen what the board can do.

---

## Full expansion map

```
      J1 (left)                     J2 (right)
   1  GND                        1  GND
   2  +3V3                       2  +3V3
   3  PB10                       3  PA13  (also SWDIO, DEBUG-2)
   4  PB2                        4  PA14  (also SWCLK, DEBUG-3)
   5  PB1                        5  PA15
   6  PB0                        6  PB3
   7  PA7                        7  PB4
   8  PA6                        8  PB5
   9  PA5                        9  PB9
```

`DEBUG`: `1 = +3V3`, `2 = SWDIO (PA13)`, `3 = SWCLK (PA14)`, `4 = GND`.

Because PA13/PA14 appear on both the DEBUG header and J2, using J2-3/J2-4 as
ordinary GPIO drops any attached debugger. The console's `gpio` command
requires an explicit `force` argument before it will reconfigure them.

There is no NRST pin on the DEBUG header, so an SWD probe cannot pull the
board out of a bad state by asserting reset — that means the RESET button.
Adding NRST to a 5-pin DEBUG header in V2 would make debugging noticeably
less fiddly.

---

## Peripheral capabilities the pin map allows

Useful to know before picking a project for this board:

| Bus | Pins available | Notes |
|-----|----------------|-------|
| I2C1 | PB6/PB7 | shared with the IMU; pull-ups already fitted |
| I2C3 | PA8/PB5 | PA8 is not broken out, so SCL is unavailable |
| SPI1 | PA5 SCK, PA6 MISO, PA7 MOSI | **all three on J1-9/8/7** — fully usable |
| SPI3 | PB3 SCK, PB4 MISO, PB5 MOSI | **all three on J2-6/7/8** — fully usable |
| USART1 | PB6 TX, PB7 RX | conflicts with the IMU bus |
| USART2 | PA2 TX, PA3 RX | conflicts with the button and LED |
| USART3 | PB10 TX, PB11 RX | TX on J1-3, **RX not broken out** |
| LPUART1 | PA2/PA3 | same conflict as USART2 |
| TIM2 CH1-4 | PA5, PA1, PA2, PA3 | CH1 free on J1-9 |
| TIM3 CH1-4 | PA6, PA7, PB0, PB1 | **all four on J1-8/7/6/5** — good for motors/servos |
| ADC | PB0, PB1, PB2 | three ADC-capable pins, on J1-6/5/4 (check the datasheet for exact channel numbers) |

**TIM3's four channels and SPI1 both landing entirely on J1** is the nicest
accident of this layout — one header gives you either four PWM outputs or a
complete SPI bus.
