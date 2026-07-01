from fpdf import FPDF

OUT = r"C:\Users\PYTHON\Desktop\fazer_3.5display\d3\UseCases.pdf"

# ── Helpers ──────────────────────────────────────────────────────────────────
class PDF(FPDF):
    def header(self):
        self.set_font("Helvetica", "B", 9)
        self.set_fill_color(20, 35, 60)
        self.set_text_color(255, 255, 255)
        self.cell(0, 7, "  Train GPS Tracker -- ESP32-S3  |  Application Use Case Document", fill=True,
                  new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def footer(self):
        self.set_y(-12)
        self.set_font("Helvetica", "I", 8)
        self.set_text_color(150, 150, 150)
        self.cell(0, 8, f"Page {self.page_no()}", align="C")

    def h1(self, txt):
        self.set_font("Helvetica", "B", 13)
        self.set_fill_color(30, 60, 110)
        self.set_text_color(255, 255, 255)
        self.cell(0, 8, f"  {txt}", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def h2(self, txt):
        self.set_font("Helvetica", "B", 11)
        self.set_fill_color(200, 215, 240)
        self.set_text_color(20, 40, 90)
        self.cell(0, 7, f"  {txt}", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def h3(self, txt):
        self.set_font("Helvetica", "B", 10)
        self.set_fill_color(235, 240, 250)
        self.set_text_color(40, 60, 100)
        self.cell(0, 6, f"    {txt}", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)

    def body(self, txt, indent=4):
        self.set_font("Helvetica", "", 9)
        self.set_text_color(30, 30, 30)
        self.set_x(self.l_margin + indent)
        self.multi_cell(0, 5, txt)
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def bullet(self, items, indent=8):
        self.set_font("Helvetica", "", 9)
        self.set_text_color(30, 30, 30)
        for item in items:
            self.set_x(self.l_margin + indent)
            self.multi_cell(0, 5, f"- {item}")
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def kv(self, key, val, indent=8):
        self.set_font("Helvetica", "B", 9)
        self.set_x(self.l_margin + indent)
        self.cell(36, 5, key + ":")
        self.set_font("Helvetica", "", 9)
        self.multi_cell(0, 5, val)

    def uc_table(self, rows):
        """Two-column table for use case fields."""
        W = 186  # usable page width (210 - 12 - 12)
        lw, vw = 38, W - 38
        self.set_font("Helvetica", "", 9)
        for label, value in rows:
            x0 = self.l_margin
            self.set_x(x0)
            self.set_fill_color(230, 235, 248)
            self.set_font("Helvetica", "B", 9)
            self.cell(lw, 6, f"  {label}", border=1, fill=True)
            self.set_fill_color(255, 255, 255)
            self.set_font("Helvetica", "", 9)
            self.multi_cell(vw, 6, f"  {value}", border=1)
            # after multi_cell x is reset; restore left margin
            self.set_x(x0)
        self.ln(3)

    def flow_box(self, steps):
        """Simple numbered flow steps."""
        self.set_font("Helvetica", "", 9)
        self.set_fill_color(245, 248, 255)
        for i, s in enumerate(steps, 1):
            self.set_x(self.l_margin + 8)
            self.set_fill_color(30, 60, 110)
            self.set_text_color(255, 255, 255)
            self.cell(8, 5, str(i), fill=True, align="C")
            self.set_fill_color(245, 248, 255)
            self.set_text_color(30, 30, 30)
            self.multi_cell(0, 5, f"  {s}", fill=True)
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def alt_flow(self, steps):
        self.set_font("Helvetica", "I", 9)
        self.set_text_color(120, 60, 0)
        for s in steps:
            self.set_x(self.l_margin + 12)
            self.multi_cell(0, 5, f"* {s}")
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def note(self, txt):
        self.set_font("Helvetica", "I", 8.5)
        self.set_fill_color(255, 250, 220)
        self.set_text_color(100, 70, 0)
        self.set_x(self.l_margin + 4)
        self.multi_cell(0, 5, f"NOTE: {txt}", fill=True)
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def screen_box(self, name, desc):
        self.set_fill_color(40, 40, 60)
        self.set_text_color(0, 200, 200)
        self.set_font("Courier", "B", 9)
        self.cell(0, 6, f"  [SCREEN: {name}]  {desc}", fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_text_color(0, 0, 0)
        self.ln(1)


pdf = PDF(orientation="P", unit="mm", format="A4")
pdf.set_auto_page_break(auto=True, margin=14)
pdf.set_left_margin(12)
pdf.set_right_margin(12)
pdf.add_page()

# ═══════════════════════════════════════════════════════════════════════════════
#  COVER BLOCK
# ═══════════════════════════════════════════════════════════════════════════════
pdf.set_fill_color(20, 35, 60)
pdf.rect(0, 0, 210, 60, "F")
pdf.set_y(10)
pdf.set_font("Helvetica", "B", 20)
pdf.set_text_color(0, 200, 220)
pdf.cell(0, 10, "Train GPS Tracker", align="C", new_x="LMARGIN", new_y="NEXT")
pdf.set_font("Helvetica", "B", 13)
pdf.set_text_color(200, 220, 255)
pdf.cell(0, 8, "Application Use Case Document", align="C", new_x="LMARGIN", new_y="NEXT")
pdf.set_font("Helvetica", "", 9)
pdf.set_text_color(160, 180, 210)
pdf.cell(0, 6, "ESP32-S3  |  ILI9488 3.5\" TFT  |  L89H GPS  |  MAX98357A Audio", align="C",
         new_x="LMARGIN", new_y="NEXT")
pdf.set_text_color(0, 0, 0)
pdf.set_y(65)

# ═══════════════════════════════════════════════════════════════════════════════
#  1. OVERVIEW
# ═══════════════════════════════════════════════════════════════════════════════
pdf.h1("1. System Overview")
pdf.body(
    "The Train GPS Tracker is an embedded device installed in a train passenger "
    "compartment. It displays the next upcoming station name, the real-time distance "
    "to that station (in metres), and the current train speed. As the train approaches "
    "a station within a configurable distance threshold it plays a pre-recorded audio "
    "announcement through a speaker. The device is operated entirely through a 3.5-inch "
    "colour touchscreen with no physical buttons required."
)

pdf.h2("1.1  Key Features")
pdf.bullet([
    "Live GPS tracking via L89H module (UART, 9600 baud)",
    "Next station display with automatic advance when within 150 m of the station",
    "GPS-based smart trip start -- picks nearest station from current position",
    "Configurable audio announcement: distance, repetitions, gap (all saved to NVS)",
    "Trip data stored on MicroSD card as JSON; downloadable over WiFi from cloud API",
    "On-screen QWERTY + symbol keyboard (3 modes: UPPER / lower / symbols)",
    "Always-visible header: GPS status, WiFi strength, volume %, battery %",
    "Battery level monitoring via ADC voltage divider (EMA-filtered, 32-sample average)",
    "Up to 3 saved WiFi networks; automatic reconnect on boot",
])

pdf.h2("1.2  Actors")
rows = [
    ("User / Operator", "Person who powers on the device, enters the trip number, and adjusts settings."),
    ("GPS Satellite",   "External system providing location, speed, and fix-quality data via NMEA sentences."),
    ("WiFi Network",    "External 802.11 network used to download trip route data from the cloud API."),
    ("MicroSD Card",    "Local storage for trip JSON files and WAV audio announcement files."),
    ("Cloud API",       "Remote HTTP server that provides station data for a given trip number in JSON format."),
]
pdf.uc_table(rows)

pdf.h2("1.3  Screen Map")
pdf.set_font("Courier", "", 8)
pdf.set_fill_color(240, 240, 240)
lines = [
    "  Boot",
    "   |",
    "   +-- Splash Screen (progress bar)",
    "        |",
    "        +-- Main Menu",
    "             |",
    "             +--[Trip]-----> Trip Search Screen",
    "             |                    |",
    "             |              (trip found) --> Station Tracking Screen",
    "             |                    |",
    "             |              (not found) --> Not Found Screen",
    "             |                                   |",
    "             |                             [DOWNLOAD] --> download via WiFi",
    "             |",
    "             +--[Settings]--> Settings Screen",
    "                                   |",
    "                                   +-- Volume Screen",
    "                                   |",
    "                                   +-- Announcement Settings Screen",
    "                                   |",
    "                                   +-- WiFi Screen",
    "                                            |",
    "                                            +-- Add SSID (keyboard)",
    "                                                     |",
    "                                                     +-- Add Password (keyboard)",
]
for l in lines:
    pdf.set_x(pdf.l_margin)
    pdf.cell(0, 4.5, l, fill=True, new_x="LMARGIN", new_y="NEXT")
pdf.set_text_color(0, 0, 0)
pdf.ln(4)

# ═══════════════════════════════════════════════════════════════════════════════
#  2. USE CASES
# ═══════════════════════════════════════════════════════════════════════════════
pdf.add_page()
pdf.h1("2. Use Cases")

# ── UC-01 ─────────────────────────────────────────────────────────────────────
pdf.h2("UC-01  System Startup & Splash Screen")
pdf.screen_box("SPLASH", "Full-screen loading screen shown at boot")
pdf.uc_table([
    ("Actor",       "Device (automatic on power-on)"),
    ("Goal",        "Initialise all hardware and show readiness status before entering the main menu."),
    ("Precondition","Device is powered on."),
    ("Postcondition","All hardware is ready; Main Menu is displayed."),
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "TFT display initialises; splash background and progress bar appear.",
    "Touch controller (XPT2046) initialised via SPI.",
    "SD card initialised (SPI bus switches to SD MISO GPIO40). Status shown: 'SD Card OK' or 'SD Card FAILED'.",
    "GPS UART1 opened at 9600 baud with 512-byte RX buffer.",
    "Preferences (NVS) opened; saved WiFi networks loaded.",
    "WiFi connection attempt started in background for first saved network.",
    "Battery ADC configured (11 dB attenuation); initial battery % read.",
    "Audio output (I2S, MAX98357A) initialised; saved volume level applied.",
    "Progress bar reaches 100% -- 'Ready!' displayed briefly.",
    "Main Menu is drawn.",
])
pdf.h3("Alternative Flows")
pdf.alt_flow([
    "SD card not detected: progress shows 'SD Card FAILED'; device continues without SD. Trip search will fail.",
    "No WiFi networks configured: 'No WiFi Configured' shown; device continues offline.",
    "GPS not connected: device continues; GPS status header shows 'No GPS'.",
])

# ── UC-02 ─────────────────────────────────────────────────────────────────────
pdf.h2("UC-02  Navigate Main Menu")
pdf.screen_box("MAIN MENU", "Two large circular icon buttons: Trip | Settings")
pdf.uc_table([
    ("Actor",        "User"),
    ("Goal",         "Select either the Trip tracking function or the Settings screen."),
    ("Precondition", "Device is showing the Main Menu."),
    ("Postcondition","User is taken to the selected screen."),
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "User views two circular menu buttons: 'Trip' (train icon) and 'Settings' (gear icon).",
    "User taps 'Trip' button -> Trip Search screen is shown.",
    "  OR  User taps 'Settings' button -> Settings screen is shown.",
])
pdf.note("The header bar is always visible showing: GPS status | WiFi signal | Volume % | Battery %.")

# ── UC-03 ─────────────────────────────────────────────────────────────────────
pdf.add_page()
pdf.h2("UC-03  Search and Start a Trip")
pdf.screen_box("TRIP SEARCH", "Input field + on-screen keyboard + round search button")
pdf.uc_table([
    ("Actor",        "User"),
    ("Goal",         "Enter a trip/route number and load its station list from the SD card."),
    ("Precondition", "SD card inserted with a JSON file named <trip_number>.json in the root folder."),
    ("Postcondition","Station Tracking screen shows the correct next station for the trip."),
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "Trip Search screen displays an input field (top) and full QWERTY keyboard below.",
    "User types the trip/route number using the keyboard (e.g. '40001').",
    "    Keyboard supports: UPPER letters, lower letters, symbols (cycle with SHIFT key).",
    "    Backspace key (<< on last row) deletes the last character.",
    "    CLR key clears the entire input.",
    "User taps the round search button (right of input field).",
    "Device reads the matching JSON file from SD card and loads all stations into RAM.",
    "GPS position is checked: nearest station is found using Haversine distance.",
    "    If within 150 m of nearest station -> next station is set to the one after it.",
    "    If farther -> nearest station itself is set as the next station.",
    "    If no GPS fix -> trip starts from station 2 (first station after origin).",
    "Station Tracking screen is displayed.",
])
pdf.h3("Alternative Flows")
pdf.alt_flow([
    "Trip number not in input: search button press is ignored.",
    "JSON file not found on SD: Not Found screen is shown with a DOWNLOAD button.",
    "JSON parse error: Not Found screen is shown.",
    "BACK key on keyboard bottom row: returns to Main Menu and clears the input field.",
])
pdf.note(
    "JSON format: root array of objects with fields: seqNo, name, lat, lon, code, engUrl. "
    "File must be placed in the SD card root as <tripnum>.json (e.g. /40001.json)."
)

# ── UC-04 ─────────────────────────────────────────────────────────────────────
pdf.h2("UC-04  Live Station Tracking")
pdf.screen_box("STATION TRACKING", "Trip number | Divider | Station name | Distance | Speed | Buttons")
pdf.uc_table([
    ("Actor",        "User (passive), GPS Satellite (automatic)"),
    ("Goal",         "Display the upcoming station in real time; automatically advance as train moves."),
    ("Precondition", "A trip has been loaded successfully (UC-03)."),
    ("Postcondition","User always sees the next approaching station and live distance/speed."),
])
pdf.h3("Display Layout")
pdf.bullet([
    "Line 1 (large cyan): Trip/route number",
    "Horizontal divider",
    "Line 2 (large white, size 5): Next station name",
    "Line 3 (green): Distance to next station in metres (e.g. '1432 m')",
    "Line 4 (yellow): Current train speed (e.g. '82.4 km/h')",
    "Bottom: '< BACK' button | 'PLAY' test button",
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "Screen displays current trip number and the next upcoming station name.",
    "GPS updates arrive every ~1 second over UART1.",
    "For each GPS fix: Haversine distance to next station is calculated in metres.",
    "If distance < 150 m: station is considered reached.",
    "    Current station name is saved as 'last passed' station.",
    "    Next station in sequence is loaded from the in-RAM cache (O(1), no SD access).",
    "    Any in-progress announcement is stopped and reset.",
    "    Screen updates immediately to show the new next station.",
    "Distance display updates when change >= 5 m (hysteresis to avoid screen flicker).",
    "Speed display updates every GPS cycle.",
    "When the last station is reached: screen shows 'Last Station' + final station name.",
])
pdf.h3("Alternative Flows")
pdf.alt_flow([
    "No GPS fix: distance shows '-- m', speed shows '-- km/h'; station name stays unchanged.",
    "GPS fix regained: header updates to 'GPS OK', distance and speed resume.",
    "Train stopped AT station: distance < 150 m is still detected; screen advances to next station.",
    "User taps BACK: returns to Trip Search screen (GPS tracking continues in background).",
    "User taps PLAY: plays the WAV announcement for the current next station once (test mode).",
])

# ── UC-05 ─────────────────────────────────────────────────────────────────────
pdf.add_page()
pdf.h2("UC-05  Audio Station Announcement")
pdf.uc_table([
    ("Actor",        "System (automatic, triggered by GPS distance)"),
    ("Goal",         "Play a pre-recorded audio announcement when the train approaches the next station."),
    ("Precondition", "SD card has a WAV file named /<station_code>.wav; trip is active with GPS fix."),
    ("Postcondition","Announcement plays the configured number of times; station is marked as announced."),
])
pdf.h3("Default Settings (all configurable via UC-08)")
pdf.bullet([
    "Announcement distance: 2000 m from the station",
    "Number of repetitions: 4 plays",
    "Gap between plays: 10 seconds",
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "During live tracking (UC-04), GPS distance to next station drops below announce distance.",
    "System checks: announced flag is false AND station has a valid code AND SD card is ready.",
    "SPI bus is switched to SD MISO (GPIO 40).",
    "WAV file /<station_code>.wav is opened via AudioFileSourceSD.",
    "AudioGeneratorWAV begins streaming to MAX98357A via I2S (GPIO 5/6/7).",
    "'announced' flag is set to true; remaining repeat count set to (reps - 1).",
    "After first play completes, gap timer starts (announceGapSec seconds).",
    "When gap expires, WAV plays again. Repeated until repeat count reaches zero.",
    "After all repetitions: SPI bus switches back to Touch MISO (GPIO 13).",
    "Touch input is unblocked (was blocked during audio playback).",
])
pdf.h3("Alternative Flows")
pdf.alt_flow([
    "WAV file not found on SD: announcement skipped silently; repeat count reset to 0.",
    "Station advances (train passes through) while audio playing: audio stopped immediately.",
    "Volume is 0: audio plays silently but all state changes still occur.",
    "User presses PLAY button: plays announcement once without setting 'announced' flag (for testing).",
])
pdf.note(
    "WAV files must be placed in SD card root as /<code>.wav where <code> matches the "
    "'code' field in the trip JSON (e.g. /MSB.wav for station code MSB). "
    "Touch is blocked during audio playback to prevent SPI bus conflicts."
)

# ── UC-06 ─────────────────────────────────────────────────────────────────────
pdf.h2("UC-06  Download Trip Data via WiFi")
pdf.screen_box("NOT FOUND", "Error icon | Trip name | BACK button | DOWNLOAD TRIP DATA button")
pdf.uc_table([
    ("Actor",        "User, WiFi Network, Cloud API"),
    ("Goal",         "Download station data for a trip that is not on the SD card."),
    ("Precondition", "Device is connected to WiFi; trip number has been entered."),
    ("Postcondition","Trip JSON saved to SD card; Station Tracking screen loads automatically."),
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "Not Found screen shows the trip number and a 'DOWNLOAD TRIP DATA' button.",
    "User taps the DOWNLOAD button.",
    "Device checks WiFi connection. Status message 'Connecting...' appears.",
    "HTTP GET sent to: https://api.mindcoinapps.com/.../downloadStationsByRouteNameJson/<trip>",
    "Server responds with full JSON payload (HTTP 200). Status: 'Receiving...'",
    "Full payload received into RAM as a String to avoid SD write races.",
    "SPI switches to SD; SD card re-initialised.",
    "Old JSON file removed; new file written to /<trip>.json on SD. Status: 'Saving to SD...'",
    "SPI switches back to Touch.",
    "searchTrip() is called with the new file. Status: 'Saved! Loading trip...'",
    "Station Tracking screen opens automatically.",
])
pdf.h3("Alternative Flows")
pdf.alt_flow([
    "No WiFi: error 'No WiFi - add in Settings' shown for 2.5 s; user returned to Not Found screen.",
    "HTTP error (non-200): error message shown with HTTP code; user stays on Not Found screen.",
    "Empty response: 'Empty response' error shown.",
    "SD init failed: 'SD init failed' shown; file not saved.",
    "SD write returns 0 bytes: 'SD write 0 bytes' shown.",
])

# ── UC-07 ─────────────────────────────────────────────────────────────────────
pdf.add_page()
pdf.h2("UC-07  Adjust Audio Volume")
pdf.screen_box("VOLUME", "Title | Percentage | Slider bar | '-' button | '+' button | BACK")
pdf.uc_table([
    ("Actor",        "User"),
    ("Goal",         "Set the speaker output volume and save it persistently."),
    ("Precondition", "User has navigated Settings -> Volume."),
    ("Postcondition","Volume applied immediately to I2S output; saved to NVS flash."),
])
pdf.h3("Normal Flow")
pdf.flow_box([
    "Volume screen shows current level as percentage and a filled progress bar.",
    "User taps '+' button: volume increases by 5% (capped at 100%).",
    "User taps '-' button: volume decreases by 5% (floored at 0%).",
    "User taps anywhere on the slider bar: volume jumps to the tapped position.",
    "Volume is applied instantly to AudioOutputI2S SetGain (0.0 at 0%, 1.0 at 50%, 2.0 at 100%).",
    "New value saved to NVS key 'volume'. Loaded on every boot.",
    "User taps BACK: returns to Settings screen.",
])
pdf.h3("Header Icon")
pdf.body("The sound icon and percentage in the header bar update immediately on every volume change.")

# ── UC-08 ─────────────────────────────────────────────────────────────────────
pdf.h2("UC-08  Configure Announcement Settings")
pdf.screen_box("ANNOUNCEMENT", "3 rows: Dist (m) | Repeats | Gap (s)  each with '-' and '+' buttons")
pdf.uc_table([
    ("Actor",        "User"),
    ("Goal",         "Tune when and how many times the station announcement plays."),
    ("Precondition", "User has navigated Settings -> Announcement."),
    ("Postcondition","New values saved to NVS; applied immediately to all subsequent announcements."),
])
pdf.h3("Configurable Parameters")
params = [
    ("Dist (m)",  "500 m", "9000 m", "500 m step", "2000 m",
     "Distance from next station at which announcement first triggers."),
    ("Repeats",   "1",     "8",      "1 step",      "4",
     "Number of times the WAV file is played."),
    ("Gap (s)",   "5 s",   "60 s",   "5 s step",    "10 s",
     "Pause between each repetition."),
]
pdf.set_font("Helvetica", "B", 8.5)
pdf.set_fill_color(30, 60, 110)
pdf.set_text_color(255, 255, 255)
for h, w in zip(["Parameter","Min","Max","Step","Default","Description"],
                [28, 14, 14, 16, 18, 96]):
    pdf.cell(w, 6, f" {h}", border=1, fill=True)
pdf.ln()
for i, row in enumerate(params):
    bg = (240, 245, 255) if i % 2 == 0 else (255, 255, 255)
    pdf.set_fill_color(*bg)
    pdf.set_text_color(0, 0, 0)
    pdf.set_font("Helvetica", "", 8.5)
    for val, w in zip(row, [28, 14, 14, 16, 18, 96]):
        pdf.cell(w, 6, f" {val}", border=1, fill=True)
    pdf.ln()
pdf.ln(3)
pdf.h3("Normal Flow")
pdf.flow_box([
    "Announcement screen shows three rows, each with the parameter name, current value, '-' and '+' buttons.",
    "User taps '+' or '-' on any row to adjust the value within its allowed range.",
    "Value updates on screen immediately.",
    "User taps BACK: all three values are saved to NVS ('annDist', 'annReps', 'annGapSec').",
])

# ── UC-09 ─────────────────────────────────────────────────────────────────────
pdf.add_page()
pdf.h2("UC-09  Manage WiFi Networks")
pdf.screen_box("WIFI", "List of saved networks (max 3) with signal status | Add New button | BACK")
pdf.uc_table([
    ("Actor",        "User, WiFi Network"),
    ("Goal",         "View, add, connect to, or delete saved WiFi networks."),
    ("Precondition", "User has navigated Settings -> WiFi."),
    ("Postcondition","Selected network is connected or deleted; changes persisted to NVS."),
])
pdf.h3("Normal Flow -- View / Connect / Delete")
pdf.flow_box([
    "WiFi screen lists up to 3 saved networks.",
    "Each row shows: SSID name | signal indicator (if currently connected).",
    "Tapping a row that is NOT the active connection: device connects to that network.",
    "Tapping a row that IS the active connection: network is deleted from the saved list.",
    "WiFi header icon updates to reflect new connection state.",
])
pdf.h3("Normal Flow -- Add New Network")
pdf.flow_box([
    "User taps 'Add New WiFi' button at bottom of WiFi screen.",
    "SSID entry screen opens (keyboard in UPPER mode).",
    "User types the network SSID and taps 'OK' circle button.",
    "SSID is saved in pendingSSID buffer; Password entry screen opens.",
    "User types the network password and taps 'SAVE' circle button.",
    "Network (SSID + password) is added to the saved list and persisted to NVS.",
    "Device immediately attempts to connect to the new network.",
    "WiFi screen is shown with updated list.",
])
pdf.h3("Alternative Flows")
pdf.alt_flow([
    "Saved list is full (3 networks): 'Add New WiFi' button still shown; oldest is not auto-deleted. "
    "User must first delete a network before adding.",
    "BACK on SSID screen: returns to WiFi screen without adding.",
    "BACK on Password screen: returns to SSID screen with the already-typed SSID restored.",
    "Device can store up to 3 networks (WIFI_MAX_NETS = 3).",
])
pdf.note("Passwords are stored in NVS as plain text. Max SSID length: 32 chars. Max password: 64 chars.")

# ── UC-10 ─────────────────────────────────────────────────────────────────────
pdf.h2("UC-10  Monitor Status Header")
pdf.uc_table([
    ("Actor",        "System (automatic, continuous)"),
    ("Goal",         "Always show live GPS status, WiFi strength, volume, and battery level at the top."),
    ("Precondition", "Device is powered on."),
    ("Postcondition","Header reflects current state within the update interval."),
])
pdf.h3("Header Zones (left to right, 480 px wide)")
pdf.set_font("Helvetica", "B", 8.5)
pdf.set_fill_color(30, 60, 110)
pdf.set_text_color(255, 255, 255)
for h, w in zip(["Zone","Pixels","Content","Update Interval"], [28, 26, 90, 42]):
    pdf.cell(w, 6, f" {h}", border=1, fill=True)
pdf.ln()
zones = [
    ("GPS",     "4-36",    "Satellite icon",                    "Constant"),
    ("Signal",  "40-160",  "No GPS (red) / GPS Weak (orange) / GPS OK (green)", "Each GPS fix"),
    ("WiFi",    "164-250", "WiFi icon + ---/Low/OK/Hi/Full",    "Every 5 s"),
    ("Sound",   "253-349", "Speaker icon + volume %",           "On volume change"),
    ("Battery", "349-476", "Battery % + battery icon",          "Every 5 s"),
]
for i, row in enumerate(zones):
    bg = (240, 245, 255) if i % 2 == 0 else (255, 255, 255)
    pdf.set_fill_color(*bg); pdf.set_text_color(0, 0, 0); pdf.set_font("Helvetica", "", 8.5)
    for val, w in zip(row, [28, 26, 90, 42]):
        pdf.cell(w, 6, f" {val}", border=1, fill=True)
    pdf.ln()
pdf.ln(3)

pdf.h3("Battery Monitor Detail")
pdf.bullet([
    "32-sample ADC read every 5 seconds on GPIO1 (voltage divider: 2x 100 kOhm from VBAT).",
    "Middle 50% of samples averaged (outlier rejection), then EMA-filtered (75/25 weight).",
    "Range: 1862 (empty, 3.0V) to 2610 (full, 4.2V) ADC counts with ADC_11db attenuation.",
])

# ═══════════════════════════════════════════════════════════════════════════════
#  3. NON-FUNCTIONAL REQUIREMENTS
# ═══════════════════════════════════════════════════════════════════════════════
pdf.add_page()
pdf.h1("3. Non-Functional Requirements")

pdf.h2("3.1  Performance")
pdf.bullet([
    "Station advance (loadStation): O(1) RAM cache lookup -- no SD access during trip.",
    "GPS processing: completes within one UART byte window (~1 ms) to prevent 512-byte buffer overflow.",
    "Distance calculation (Haversine): computed once per GPS fix, result shared between advance check and display.",
    "Display refresh: distance updates only when change >= 5 m to minimise SPI bus load.",
    "Audio decode: WAV streaming via I2S; processed in loop() to keep latency below 1 audio buffer.",
])

pdf.h2("3.2  Memory")
pdf.bullet([
    "Station cache: 100 entries x 84 bytes = ~8.4 KB static RAM (no heap allocation during trip).",
    "JSON document: allocated in stack during searchTrip(), freed after cache is populated.",
    "WiFi payload: buffered as String in RAM before SD write to avoid SPI race conditions.",
    "Battery sample array: static (32 ints = 128 bytes), not stack-allocated.",
])

pdf.h2("3.3  Persistence (NVS via Preferences)")
rows_nv = [
    ("volume",       "int",   "Audio volume 0-100",            "50"),
    ("annDist",      "float", "Announcement trigger distance", "2000.0 m"),
    ("annReps",      "int",   "Announcement repetitions",      "4"),
    ("annGapSec",    "int",   "Gap between repetitions",       "10 s"),
    ("wifiSSID0..2", "str",   "Saved WiFi SSIDs",              "(empty)"),
    ("wifiPass0..2", "str",   "Saved WiFi passwords",          "(empty)"),
    ("wifiCount",    "int",   "Number of saved networks",      "0"),
]
pdf.set_font("Helvetica", "B", 8.5)
pdf.set_fill_color(30, 60, 110); pdf.set_text_color(255, 255, 255)
for h, w in zip(["NVS Key", "Type", "Description", "Default"], [42, 18, 90, 36]):
    pdf.cell(w, 6, f" {h}", border=1, fill=True)
pdf.ln()
for i, row in enumerate(rows_nv):
    bg = (240, 245, 255) if i % 2 == 0 else (255, 255, 255)
    pdf.set_fill_color(*bg); pdf.set_text_color(0, 0, 0); pdf.set_font("Helvetica", "", 8.5)
    for val, w in zip(row, [42, 18, 90, 36]):
        pdf.cell(w, 6, f" {val}", border=1, fill=True)
    pdf.ln()
pdf.ln(3)

pdf.h2("3.4  SD Card Data Format")
pdf.set_font("Courier", "", 8)
pdf.set_fill_color(245, 245, 245)
for l in [
    '  File location: SD root, filename = <tripnumber>.json',
    '  Example: /40001.json',
    '',
    '  [',
    '    { "seqNo":1, "name":"Chennai Beach Jn", "lat":13.091,',
    '      "lon":80.292, "code":"MSB", "engUrl":"..." },',
    '    { "seqNo":2, "name":"Chennai Fort",',
    '      "lat":13.083, "lon":80.283, "code":"MSF", "engUrl":"..." },',
    '    ...',
    '  ]',
]:
    pdf.set_x(pdf.l_margin)
    pdf.cell(0, 4.5, l, fill=True, new_x="LMARGIN", new_y="NEXT")
pdf.set_text_color(0, 0, 0); pdf.ln(3)

pdf.h2("3.5  Audio File Format")
pdf.bullet([
    "Format: WAV (PCM), mono, any sample rate supported by ESP8266Audio.",
    "Location: SD card root, filename = /<station_code>.wav",
    "Example: /MSB.wav for station code 'MSB'.",
    "Files are opened fresh for each repetition (AudioFileSourceSD re-opened per play).",
])

pdf.h2("3.6  SPI Bus Sharing")
pdf.body(
    "The ILI9488 display, XPT2046 touch controller, and SD card module all share one SPI bus "
    "(SCK=GPIO12, MOSI=GPIO11) with separate CS pins and separate MISO lines. "
    "The firmware calls SPI.end() + SPI.begin() to switch the active MISO before each SD or touch "
    "operation. Touch input is blocked (loop() returns early) while audio is playing because the "
    "SD MISO must remain active for audio streaming."
)

# ═══════════════════════════════════════════════════════════════════════════════
#  4. SCREEN REFERENCE
# ═══════════════════════════════════════════════════════════════════════════════
pdf.add_page()
pdf.h1("4. Screen Reference")

screens = [
    ("0 - Main Menu",       "SCREEN_MENU",     "Trip button + Settings button. Header bar always shown."),
    ("1 - Trip Search",     "SCREEN_TRIP",     "Text input field + on-screen keyboard (UPPER/lower/symbols). "
                                                "Round search button. BACK key returns to Main Menu."),
    ("2 - Station Track",   "SCREEN_STATION",  "Trip number (cyan, large) | divider | next station name (white, "
                                                "large) or last station (orange) | distance in m (green) | speed "
                                                "km/h (yellow) | BACK + PLAY buttons at bottom."),
    ("3 - Not Found",       "SCREEN_NOT_FOUND","Red X icon | 'Trip Not Found' message | trip number | "
                                                "BACK button | DOWNLOAD TRIP DATA button."),
    ("4 - Settings",        "SCREEN_SETTINGS", "Three option rows: Audio Volume | Announcement | Wifi Setting. "
                                                "BACK button returns to Main Menu."),
    ("5 - Volume",          "SCREEN_VOLUME",   "Title | percentage (large green) | filled progress bar | "
                                                "'-' button (left) | '+' button (right) | BACK."),
    ("6 - WiFi List",       "SCREEN_WIFI",     "List of saved networks (connected one highlighted) | "
                                                "Add New WiFi button | BACK."),
    ("7 - WiFi SSID Input", "SCREEN_WIFI_SSID","Input field | QWERTY keyboard | 'OK' circle button | BACK."),
    ("8 - WiFi Pass Input", "SCREEN_WIFI_PASS","Input field (password) | QWERTY keyboard | 'SAVE' circle button "
                                                "| BACK (restores SSID input)."),
    ("9 - Announcement",    "SCREEN_ANNOUNCE", "Three rows (Dist/Repeats/Gap), each with '-' value '+' buttons. "
                                                "BACK saves settings to NVS."),
]
pdf.set_font("Helvetica", "B", 8.5)
pdf.set_fill_color(30, 60, 110); pdf.set_text_color(255, 255, 255)
for h, w in zip(["Screen","Constant","Description"], [36, 38, 112]):
    pdf.cell(w, 6, f" {h}", border=1, fill=True)
pdf.ln()
for i, (name, const, desc) in enumerate(screens):
    x0 = pdf.l_margin
    bg = (240, 245, 255) if i % 2 == 0 else (255, 255, 255)
    pdf.set_fill_color(*bg); pdf.set_text_color(0, 0, 0); pdf.set_font("Helvetica", "", 8.5)
    pdf.set_x(x0)
    pdf.cell(36,  6, f" {name}",  border=1, fill=True)
    pdf.cell(38,  6, f" {const}", border=1, fill=True)
    pdf.multi_cell(112, 6, f" {desc}", border=1, fill=True)
    pdf.set_x(x0)
pdf.ln(4)

pdf.output(OUT)
print(f"PDF saved: {OUT}")
