from fpdf import FPDF

OUT = r"C:\Users\PYTHON\Desktop\fazer_3.5display\d3\WIRING.pdf"

class PDF(FPDF):
    def header(self):
        self.set_font("Helvetica", "B", 10)
        self.set_fill_color(30, 40, 60)
        self.set_text_color(255, 255, 255)
        self.cell(0, 8, "ESP32-S3 Train Tracker -- Pin Connection & Wiring Guide", align="C", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def footer(self):
        self.set_y(-12)
        self.set_font("Helvetica", "I", 8)
        self.set_text_color(130, 130, 130)
        self.cell(0, 8, f"Page {self.page_no()}", align="C")

    def section(self, title):
        self.set_font("Helvetica", "B", 11)
        self.set_fill_color(50, 80, 120)
        self.set_text_color(255, 255, 255)
        self.cell(0, 7, f"  {title}", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def subsection(self, title):
        self.set_font("Helvetica", "B", 10)
        self.set_fill_color(220, 230, 245)
        self.set_text_color(20, 40, 80)
        self.cell(0, 6, f"  {title}", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def note(self, text):
        self.set_font("Helvetica", "I", 8.5)
        self.set_text_color(80, 80, 80)
        self.multi_cell(0, 5, f"NOTE:  {text}")
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def table(self, headers, rows, col_widths, header_colors=(50,80,120), alt_color=(240,245,255)):
        # Header row
        self.set_font("Helvetica", "B", 8.5)
        self.set_fill_color(*header_colors)
        self.set_text_color(255, 255, 255)
        for h, w in zip(headers, col_widths):
            self.cell(w, 6, f" {h}", border=1, fill=True)
        self.ln()
        # Data rows
        self.set_font("Helvetica", "", 8.5)
        self.set_text_color(0, 0, 0)
        for i, row in enumerate(rows):
            if i % 2 == 0:
                self.set_fill_color(*alt_color)
            else:
                self.set_fill_color(255, 255, 255)
            for cell, w in zip(row, col_widths):
                self.cell(w, 6, f" {cell}", border=1, fill=True)
            self.ln()
        self.ln(3)

    def mono(self, text, bg=(245, 245, 245)):
        self.set_font("Courier", "", 8)
        self.set_fill_color(*bg)
        lines = text.strip().split("\n")
        for line in lines:
            self.cell(0, 5, line, fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_font("Helvetica", "", 9)
        self.ln(2)


pdf = PDF(orientation="P", unit="mm", format="A4")
pdf.set_auto_page_break(auto=True, margin=15)
pdf.add_page()
pdf.set_left_margin(12)
pdf.set_right_margin(12)

# ── Title block ─────────────────────────────────────────────────────────────
pdf.set_font("Helvetica", "B", 16)
pdf.set_text_color(30, 40, 80)
pdf.cell(0, 10, "ESP32-S3 Train GPS Tracker", align="C", new_x="LMARGIN", new_y="NEXT")
pdf.set_font("Helvetica", "", 11)
pdf.set_text_color(80, 80, 80)
pdf.cell(0, 7, "Pin Connection Documentation & Wiring Schematic", align="C", new_x="LMARGIN", new_y="NEXT")
pdf.ln(4)

# ── Board info ───────────────────────────────────────────────────────────────
pdf.set_font("Helvetica", "", 9)
pdf.set_text_color(0,0,0)
info = [
    ("MCU", "ESP32-S3 DevKit (38-pin)"),
    ("Display", "ILI9488 3.5\" TFT 480x320 with XPT2046 touch"),
    ("Audio", "MAX98357A I2S mono amplifier"),
    ("GPS", "L89H (UART, 9600 baud)"),
    ("Storage", "MicroSD card (SPI mode)"),
]
for k, v in info:
    pdf.set_font("Helvetica", "B", 9)
    pdf.cell(32, 5, k + ":")
    pdf.set_font("Helvetica", "", 9)
    pdf.cell(0, 5, v, new_x="LMARGIN", new_y="NEXT")
pdf.ln(4)

# ── 1. GPIO Table ─────────────────────────────────────────────────────────────
pdf.section("1. Complete GPIO Assignment")

gpio_headers = ["GPIO", "Signal", "Connected To", "Dir", "Notes"]
gpio_widths  = [14, 28, 55, 12, 77]
gpio_rows = [
    ("1",  "BAT_ADC",    "Voltage divider mid-point",    "IN",  "100kOhm/100kOhm from VBAT -- ADC 11dB"),
    ("5",  "I2S_BCLK",   "MAX98357A - BCLK",             "OUT", "I2S bit clock"),
    ("6",  "I2S_LRC",    "MAX98357A - LRC/WS",           "OUT", "I2S word select"),
    ("7",  "I2S_DIN",    "MAX98357A - DIN",              "OUT", "I2S serial audio data"),
    ("8",  "TFT_DC",     "ILI9488 - DC/RS",              "OUT", "Data/Command select"),
    ("9",  "TFT_RST",    "ILI9488 - RESET",              "OUT", "Active LOW reset"),
    ("10", "TFT_CS",     "ILI9488 - CS",                 "OUT", "TFT chip select"),
    ("11", "SPI_MOSI",   "TFT + Touch + SD (shared)",    "OUT", "Shared SPI data out"),
    ("12", "SPI_SCK",    "TFT + Touch + SD (shared)",    "OUT", "Shared SPI clock"),
    ("13", "TOUCH_MISO", "XPT2046 - MISO (DO)",          "IN",  "Touch only -- TFT SDO NOT connected"),
    ("14", "TOUCH_CS",   "XPT2046 - CS",                 "OUT", "Touch chip select"),
    ("15", "SD_CS",      "SD card module - CS",          "OUT", "SD chip select"),
    ("16", "GPS_RX",     "GPS module - TX",              "IN",  "UART1 RX"),
    ("17", "GPS_TX",     "GPS module - RX",              "OUT", "UART1 TX"),
    ("40", "SD_MISO",    "SD card module - MISO",        "IN",  "Separate MISO for SD"),
]
pdf.table(gpio_headers, gpio_rows, gpio_widths)

pdf.note("ILI9488 SDO pin must NOT be connected -- it does not tristate when CS is high and would conflict with XPT2046 MISO on GPIO13.")

# ── 2. Per-peripheral ────────────────────────────────────────────────────────
pdf.section("2. Per-Peripheral Wiring")

pdf.subsection("2a. TFT Display -- ILI9488 3.5\" (480x320)")
tft_h = ["Display Pin", "Label", "ESP32-S3 GPIO", "Notes"]
tft_w = [38, 32, 38, 78]
tft_r = [
    ("VCC",        "3.3V",     "3V3",           ""),
    ("GND",        "GND",      "GND",           ""),
    ("CS",         "LCD_CS",   "GPIO 10",       ""),
    ("RESET",      "LCD_RST",  "GPIO 9",        "Active LOW"),
    ("DC/RS",      "LCD_DC",   "GPIO 8",        "Data/Command select"),
    ("SDI (MOSI)", "LCD_MOSI", "GPIO 11",       "Shared SPI bus"),
    ("SCK",        "LCD_SCK",  "GPIO 12",       "Shared SPI bus"),
    ("LED/BL",     "Backlight","3V3 or via 33Ohm","Always-on backlight"),
    ("SDO (MISO)", "--",        "Do NOT connect","Bus conflict risk"),
]
pdf.table(tft_h, tft_r, tft_w)

pdf.subsection("2b. Touch Controller -- XPT2046 (on TFT board)")
tc_h = ["Touch Pin", "Label",  "ESP32-S3 GPIO", "Notes"]
tc_w = [38, 32, 38, 78]
tc_r = [
    ("T_CLK", "SCK",  "GPIO 12", "Shared SPI bus"),
    ("T_CS",  "CS",   "GPIO 14", "Own chip select"),
    ("T_DIN", "MOSI", "GPIO 11", "Shared SPI bus"),
    ("T_DO",  "MISO", "GPIO 13", "Separate MISO line"),
    ("T_IRQ", "IRQ",  "--",       "Not used (polling mode)"),
]
pdf.table(tc_h, tc_r, tc_w)

pdf.subsection("2c. SD Card Module")
sd_h = ["SD Pin", "Label", "ESP32-S3 GPIO", "Notes"]
sd_w = [38, 32, 38, 78]
sd_r = [
    ("VCC",  "3.3V", "3V3",     ""),
    ("GND",  "GND",  "GND",     ""),
    ("CS",   "CS",   "GPIO 15", "Own chip select"),
    ("MOSI", "MOSI", "GPIO 11", "Shared SPI bus"),
    ("CLK",  "SCK",  "GPIO 12", "Shared SPI bus"),
    ("MISO", "MISO", "GPIO 40", "Separate MISO line"),
]
pdf.table(sd_h, sd_r, sd_w)

pdf.subsection("2d. Audio Amplifier -- MAX98357A (I2S Mono)")
i2s_h = ["MAX98357A Pin", "Label", "ESP32-S3 GPIO", "Notes"]
i2s_w = [42, 28, 38, 78]
i2s_r = [
    ("VIN",        "5V",   "5V (VUSB/boost)", "Do NOT use 3.3V"),
    ("GND",        "GND",  "GND",             ""),
    ("BCLK",       "BCLK", "GPIO 5",          "I2S bit clock"),
    ("LRC / WS",   "LRC",  "GPIO 6",          "Word select"),
    ("DIN",        "DIN",  "GPIO 7",          "Serial audio data"),
    ("SD (shutdown)","--",  "Float or 3.3V",   "Float = enabled"),
    ("GAIN",       "--",    "Float",           "9 dB gain (default)"),
    ("OUT+ / OUT-","SPK",  "4-8 Ohm speaker",   "Direct speaker output"),
]
pdf.table(i2s_h, i2s_r, i2s_w)

pdf.subsection("2e. GPS Module -- L89H (UART)")
gps_h = ["GPS Pin", "Label", "ESP32-S3 GPIO", "Notes"]
gps_w = [38, 32, 38, 78]
gps_r = [
    ("VCC", "3.3V",   "3V3",     "Check module datasheet -- some need 5V"),
    ("GND", "GND",    "GND",     ""),
    ("TX",  "GPS TX", "GPIO 16", "GPS TX  ->  ESP32 RX"),
    ("RX",  "GPS RX", "GPIO 17", "GPS RX  <-  ESP32 TX"),
    ("PPS", "--",      "--",       "Not used"),
]
pdf.table(gps_h, gps_r, gps_w)
pdf.note("UART1, 9600 baud, 512-byte RX buffer. GPS TX must connect to ESP32 GPIO16 (RX), not TX.")

# ── 3. Battery divider ───────────────────────────────────────────────────────
pdf.subsection("2f. Battery Monitor (Voltage Divider on GPIO 1)")
bat_h = ["ADC Raw", "GPIO 1 Voltage", "Battery Voltage", "State"]
bat_w = [28, 38, 42, 78]
bat_r = [
    ("~2610", "~2.10 V", "4.2 V", "Fully charged"),
    ("~2298", "~1.85 V", "3.7 V", "Nominal"),
    ("~1862", "~1.50 V", "3.0 V", "Empty (protect cell)"),
]
pdf.table(bat_h, bat_r, bat_w)
pdf.note("Voltage divider: VBAT -> R1(100 kOhm) -> GPIO1 -> R2(100 kOhm) -> GND. Call analogSetAttenuation(ADC_11db) in setup().")

# ── New page for schematic ───────────────────────────────────────────────────
pdf.add_page()

# ── 3. MISO switching ────────────────────────────────────────────────────────
pdf.section("3. Shared SPI Bus & MISO Switching")
pdf.set_font("Helvetica", "", 9)
pdf.multi_cell(0, 5,
    "The TFT display, XPT2046 touch controller, and SD card all share one SPI bus "
    "(SCK=GPIO12, MOSI=GPIO11). However each device has its own MISO line, so the "
    "firmware switches the active MISO in software before each access using SPI.end() "
    "followed by SPI.begin() with the correct MISO pin. Each device also has its own "
    "dedicated CS pin to ensure only one is active at a time."
)
pdf.ln(2)
pdf.mono(
"""  Before SD access:
    SPI.end();
    SPI.begin(12, GPIO40, 11);   // SD_MISO = GPIO40

  Before Touch access:
    SPI.end();
    SPI.begin(12, GPIO13, 11);   // TOUCH_MISO = GPIO13"""
)

# ── 4. Block schematic ───────────────────────────────────────────────────────
pdf.section("4. Block Schematic Diagram")
pdf.mono(
""" +-----------------------------------------------------------------+
 |                      ESP32-S3 DevKit                            |
 |                                                                 |
 |  GPIO 1  ---[BAT_ADC]--- 100kOhm ---+--- 100kOhm --- GND      |
 |                                     |                           |
 |                                   VBAT (LiPo 3.7V)             |
 |                                                                 |
 |  GPIO 5  ---[I2S BCLK]--+                                      |
 |  GPIO 6  ---[I2S LRC ]--+--- MAX98357A --- 4~8 Ohm Speaker     |
 |  GPIO 7  ---[I2S DIN ]--+                                      |
 |                                                                 |
 |  GPIO 8  ---[TFT DC  ]--+                                      |
 |  GPIO 9  ---[TFT RST ]--+                                      |
 |  GPIO 10 ---[TFT CS  ]--+----- ILI9488 TFT 480x320             |
 |  GPIO 11 ---[MOSI    ]--+  (SDO/MISO: DO NOT CONNECT)          |
 |  GPIO 12 ---[SCK     ]--+                                      |
 |                          |                                      |
 |  GPIO 11 ---[MOSI    ]--+                                      |
 |  GPIO 12 ---[SCK     ]--+----- XPT2046 Touch Controller        |
 |  GPIO 13 ---[Touch MISO]--+                                    |
 |  GPIO 14 ---[Touch CS]--+                                      |
 |                                                                 |
 |  GPIO 11 ---[MOSI    ]--+                                      |
 |  GPIO 12 ---[SCK     ]--+----- MicroSD Card Module             |
 |  GPIO 15 ---[SD CS   ]--+                                      |
 |  GPIO 40 ---[SD MISO ]--+                                      |
 |                                                                 |
 |  GPIO 16 <--[UART1 RX]--- GPS TX                               |
 |  GPIO 17 -->[UART1 TX]--- GPS RX    (L89H)          |
 +-----------------------------------------------------------------+

  SPI Bus Summary:
    SCK  (GPIO 12) ---------> TFT,  Touch,  SD  (all 3 share this)
    MOSI (GPIO 11) ---------> TFT,  Touch,  SD  (all 3 share this)
    MISO switched by FW:
      GPIO 13 <----------- XPT2046 MISO  (active during touch reads)
      GPIO 40 <----------- SD MISO       (active during SD access)
      TFT SDO  = NOT CONNECTED"""
)

# ── 5. Power ─────────────────────────────────────────────────────────────────
pdf.section("5. Power Supply Summary")
pwr_h = ["Module", "Voltage", "Typical Current", "Source"]
pwr_w = [50, 24, 42, 70]
pwr_r = [
    ("ESP32-S3",     "3.3 V", "240 mA (WiFi on)", "DevKit on-board LDO"),
    ("ILI9488 TFT",  "3.3 V", "30-80 mA",         "ESP32 3V3 pin"),
    ("XPT2046",      "3.3 V", "~2 mA",             "ESP32 3V3 pin"),
    ("SD Card",      "3.3 V", "100-200 mA active", "ESP32 3V3 pin"),
    ("GPS L89H",   "3.3 V", "~50 mA",            "ESP32 3V3 pin"),
    ("MAX98357A",    "5 V",   "Up to 500 mA peak", "USB or 5V boost"),
    ("LiPo Battery", "3.7-4.2 V", "-- (source)",   "External charger"),
]
pdf.table(pwr_h, pwr_r, pwr_w)
pdf.note("The ESP32-S3 DevKit LDO can typically supply ~500 mA total from the 3V3 pin. If all peripherals are active simultaneously, consider an external 3.3V regulator.")
pdf.note("MAX98357A must be powered from 5V. Use a 5V boost converter if running from LiPo battery.")

# ── 6. User_Setup.h ──────────────────────────────────────────────────────────
pdf.section("6. TFT_eSPI  User_Setup.h  (key settings)")
pdf.mono(
"""  #define ILI9488_DRIVER

  #define TFT_MISO 13    // XPT2046 MISO -- TFT SDO NOT connected
  #define TFT_MOSI 11
  #define TFT_SCLK 12
  #define TFT_CS   10
  #define TFT_DC    8
  #define TFT_RST   9

  #define TOUCH_CS 14    // XPT2046 chip select"""
)

pdf.output(OUT)
print(f"PDF saved: {OUT}")


