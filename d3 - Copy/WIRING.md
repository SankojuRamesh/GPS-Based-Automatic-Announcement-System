---
noteId: "138e12b0751f11f19953a311ca2412c2"
tags: []

---

﻿# ESP32-S3 Train Tracker — Pin Connection & Wiring Guide

## Board: ESP32-S3 (38-pin DevKit)
## Display: ILI9488 3.5" TFT (480×320) with XPT2046 Touch

---

## 1. Complete GPIO Assignment Table

| GPIO | Signal      | Connected To          | Direction |
|------|-------------|-----------------------|-----------|
|  1   | BAT_ADC     | Battery voltage divider mid-point | IN (ADC) |
|  5   | I2S_BCLK    | MAX98357A – BCLK      | OUT |
|  6   | I2S_LRC     | MAX98357A – LRC/WS    | OUT |
|  7   | I2S_DOUT    | MAX98357A – DIN       | OUT |
|  8   | TFT_DC      | ILI9488 – DC/RS       | OUT |
|  9   | TFT_RST     | ILI9488 – RESET       | OUT |
| 10   | TFT_CS      | ILI9488 – CS          | OUT |
| 11   | SPI_MOSI    | TFT + Touch + SD (shared) | OUT |
| 12   | SPI_SCK     | TFT + Touch + SD (shared) | OUT |
| 13   | TOUCH_MISO  | XPT2046 – MISO (DO)   | IN  |
| 14   | TOUCH_CS    | XPT2046 – CS          | OUT |
| 15   | SD_CS       | SD card module – CS   | OUT |
| 16   | GPS_RX      | GPS module – TX       | IN  |
| 17   | GPS_TX      | GPS module – RX       | OUT |
| 40   | SD_MISO     | SD card module – MISO | IN  |

> **Note:** GPIO 13 is labeled TFT_MISO in User_Setup.h but the ILI9488 SDO
> pin must NOT be connected — ILI9488 does not tristate its SDO when CS is high,
> which would conflict with the touch controller on the same bus.
> GPIO 13 carries Touch MISO only.

---

## 2. Per-Peripheral Wiring

### 2a. TFT Display — ILI9488 3.5" (480×320)

| Display Pin | Label     | ESP32-S3 GPIO | Notes |
|-------------|-----------|---------------|-------|
| VCC         | 3.3V      | 3V3           | |
| GND         | GND       | GND           | |
| CS          | LCD_CS    | GPIO **10**   | |
| RESET       | LCD_RST   | GPIO **9**    | |
| DC/RS       | LCD_DC    | GPIO **8**    | Data/Command select |
| SDI (MOSI)  | LCD_MOSI  | GPIO **11**   | Shared SPI |
| SCK         | LCD_SCK   | GPIO **12**   | Shared SPI |
| LED/BL      | Backlight | 3V3 or 3V3 via 33Ω | Always on |
| SDO (MISO)  | —         | **Do NOT connect** | Bus conflict risk |

### 2b. Touch Controller — XPT2046 (on same TFT board)

| Touch Pin | Label  | ESP32-S3 GPIO | Notes |
|-----------|--------|---------------|-------|
| T_CLK     | SCK    | GPIO **12**   | Shared SPI |
| T_CS      | CS     | GPIO **14**   | Own CS pin |
| T_DIN     | MOSI   | GPIO **11**   | Shared SPI |
| T_DO      | MISO   | GPIO **13**   | Separate MISO line |
| T_IRQ     | IRQ    | — (not used)  | Polling mode |

### 2c. SD Card Module

| SD Pin    | Label  | ESP32-S3 GPIO | Notes |
|-----------|--------|---------------|-------|
| VCC       | 3.3V   | 3V3           | |
| GND       | GND    | GND           | |
| CS        | CS     | GPIO **15**   | Own CS pin |
| MOSI      | MOSI   | GPIO **11**   | Shared SPI |
| CLK       | SCK    | GPIO **12**   | Shared SPI |
| MISO      | MISO   | GPIO **40**   | Separate MISO line |

### 2d. Audio Amplifier — MAX98357A (I2S, Mono)

| MAX98357A Pin | Label  | ESP32-S3 GPIO | Notes |
|---------------|--------|---------------|-------|
| VIN           | 5V     | 5V (VUSB)     | |
| GND           | GND    | GND           | |
| BCLK          | BCLK   | GPIO **5**    | Bit clock |
| LRC / WS      | LRC    | GPIO **6**    | Left/Right word select |
| DIN           | DIN    | GPIO **7**    | Serial audio data |
| SD (shutdown) | —      | Leave floating or 3.3V | Float = enable |
| GAIN          | —      | Leave floating | 9 dB default |
| Speaker +/−   | SPK    | 4–8Ω speaker  | |

### 2e. GPS Module — UART (e.g. L89H)

| GPS Pin | Label  | ESP32-S3 GPIO | Notes |
|---------|--------|---------------|-------|
| VCC     | 3.3V   | 3V3           | Check module rating |
| GND     | GND    | GND           | |
| TX      | GPS TX | GPIO **16**   | GPS TX → ESP RX |
| RX      | GPS RX | GPIO **17**   | GPS RX ← ESP TX |
| PPS     | —      | Not used      | Optional |

Baud rate: **9600**, UART1, RX buffer: 512 bytes

### 2f. Battery Monitor (Voltage Divider)

```
LiPo VBAT (+) ─── R1 (100 kΩ) ──┬── R2 (100 kΩ) ─── GND
                                  │
                               GPIO 1
                              (ADC input)
```

| ADC Value | Voltage at GPIO 1 | Battery Voltage | State |
|-----------|-------------------|-----------------|-------|
| ~2610     | ~2.10 V           | 4.2 V           | Full  |
| ~2298     | ~1.85 V           | 3.7 V           | Nominal |
| ~1862     | ~1.50 V           | 3.0 V           | Empty |

> Use `analogSetAttenuation(ADC_11db)` in setup() for the 0–3.9 V input range.

---

## 3. Shared SPI Bus — MISO Switching

The TFT, Touch, and SD card all share one SPI bus (SCK=12, MOSI=11) but each
device uses its own MISO line. Firmware switches the active MISO in software
before accessing each device:

```
Before SD access:     SPI.end();  SPI.begin(12, GPIO40, 11);  // SD_MISO
Before Touch access:  SPI.end();  SPI.begin(12, GPIO13, 11);  // TOUCH_MISO
```

Each device also has its own CS pin, so only one is active at a time.

```
                      GPIO 12 (SCK)  ──────┬──────────┬──────────┐
                      GPIO 11 (MOSI) ──────┼──────────┼──────────┤
                                           │          │          │
                                       ILI9488    XPT2046    SD Card
                                       CS=GPIO10  CS=GPIO14  CS=GPIO15
                                       (MISO=N/C) MISO=GPIO13 MISO=GPIO40
```

---

## 4. ASCII Block Schematic

```
 ┌─────────────────────────────────────────────────────────────────────┐
 │                        ESP32-S3 DevKit                              │
 │                                                                     │
 │  GPIO 1  ─── BAT_ADC ──── [100k]──VBAT        GPS Module           │
 │                                └──[100k]──GND  ┌──────────┐        │
 │  GPIO 5  ─── I2S BCLK ─────────────────────►  │  L89H  │        │
 │  GPIO 6  ─── I2S LRC  ─────────────────────►  │  TX──────┼─► G16  │
 │  GPIO 7  ─── I2S DIN  ──── MAX98357A ──► SPK  │  RX──────┼─◄ G17  │
 │                             ┌──────────┐       └──────────┘        │
 │  GPIO 8  ─── TFT DC ──────►│           │                           │
 │  GPIO 9  ─── TFT RST ─────►│ ILI9488   │  SD Card Module           │
 │  GPIO 10 ─── TFT CS ──────►│ 480×320   │  ┌──────────────┐        │
 │  GPIO 11 ─── MOSI ────────►│ TFT       │  │   SD Card    │        │
 │  GPIO 12 ─── SCK  ────────►│           │  │  CS ─────────┼─◄ G15  │
 │                             ├───────────┤  │  MOSI ───────┼─◄ G11  │
 │  GPIO 11 ─── MOSI ────────►│ XPT2046   │  │  SCK ────────┼─◄ G12  │
 │  GPIO 12 ─── SCK  ────────►│ Touch     │  │  MISO ───────┼─► G40  │
 │  GPIO 13 ◄── MISO ─────────│           │  └──────────────┘        │
 │  GPIO 14 ─── Touch CS ────►│           │                           │
 │                             └───────────┘                           │
 └─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Power Supply Summary

| Module         | Voltage | Current (typical) | Source         |
|----------------|---------|-------------------|----------------|
| ESP32-S3       | 3.3V    | 240 mA (WiFi active) | LDO on DevKit |
| ILI9488 TFT    | 3.3V    | 30–80 mA          | ESP32 3V3 pin  |
| XPT2046        | 3.3V    | 2 mA              | ESP32 3V3 pin  |
| SD Card        | 3.3V    | 100–200 mA active | ESP32 3V3 pin  |
| GPS (L89H)   | 3.3V    | 50 mA             | ESP32 3V3 pin  |
| MAX98357A      | 5V      | 500 mA (peak)     | USB/5V rail    |
| LiPo battery   | 3.7–4.2V | — (source)       | External       |

> If powering from battery use a proper LiPo charger + 5V boost for MAX98357A.
> The ESP32-S3 DevKit LDO can supply ~500 mA total from its 3V3 pin.

---

## 6. Library / User_Setup.h Configuration

```cpp
// TFT_eSPI  User_Setup.h (key lines)
#define ILI9488_DRIVER

#define TFT_MISO 13    // touch MISO (TFT SDO NOT connected)
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC    8
#define TFT_RST   9

#define TOUCH_CS 14
```

