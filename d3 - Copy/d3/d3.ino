#include <TFT_eSPI.h>
#include <SPI.h>
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>
#include <math.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"
#include "satellite.h"
#include "battery_2.h"
#include "route_img.h"
#include "settings_img.h"
#include "tracking_img.h"
#include "wifi_img.h"
#include "sound_img.h"
#include "search_img.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define TOUCH_CS   14
#define TOUCH_MISO 13
#define TFT_BL_PIN 18
#define SD_CS      15
#define SD_MISO    40
// Shared SPI bus: SCK=GPIO12  MOSI=GPIO11
// Touch MISO=GPIO13, SD MISO=GPIO40 — switched in software

// StaEntry must be declared before any function prototypes that reference it
struct StaEntry { int seqNo; char name[64]; float lat, lon; char code[12]; char engUrl[192]; };

#define WIFI_MAX_NETS 3
char    wifiSSIDs[WIFI_MAX_NETS][33]  = {};
char    wifiPasses[WIFI_MAX_NETS][65] = {};
int     wifiCount     = 0;
bool    wifiConnected = false;
int     wifiSigLevel  = 0;
unsigned long lastWifiCheck = 0;

char wifiInputBuf[65] = "";
int  wifiInputLen     = 0;
char pendingSSID[33]  = "";
int  kbMode           = 0;  // 0=UPPER  1=lower  2=symbols

int        volumeLevel = 50;
Preferences prefs;
float announceDistM    = 500.0f;
float announceNearDistM = 150.0f;
int   announceReps     = 4;
int   announceGapSec   = 10;

#define BAT_ADC_PIN     1
// 3S LiPo pack — voltage divider R1=40kΩ R2=10kΩ (ratio 10/50 = 0.2)
// Full  = 3 × 4.2V = 12.6V → 12600 × 0.2 = 2520 mV at pin
// Empty = 3 × 3.0V =  9.0V →  9000 × 0.2 = 1800 mV at pin
#define BAT_MV_FULL  2520
#define BAT_MV_EMPTY 1800

// GPS — UART1  (L89H or any NMEA module)
// Wire: GPS TX → GPIO 16,  GPS RX → GPIO 17
#define GPS_RX 16
#define GPS_TX 17
HardwareSerial gpsSerial(1);
TinyGPSPlus    gps;
float          gpsSpeedKmh  = 0.0f;
float          gpsLat       = 0.0f;
float          gpsLon       = 0.0f;
bool           gpsHasFix    = false;
unsigned long  lastGpsDataMs  = 0;
unsigned long  lastGpsFixMs   = 0;   // last millis() when a valid location fix was received
int            gpsSatCount  = 0;
bool           sdReady      = false;
float          approachDistM  = 500.0f;
float          gpsMatchDistM  = 200.0f;  // radius used to pin current station on trip load

int batteryPercent = 0;
int signalLevel    = 0;
volatile bool audioStop = false;   // set true to abort current playStationAudio()

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS);

#define SCREEN_MENU       0
#define SCREEN_TRIP       1
#define SCREEN_STATION    2
#define SCREEN_NOT_FOUND  3
#define SCREEN_SETTINGS   4
#define SCREEN_VOLUME     5
#define SCREEN_WIFI       6
#define SCREEN_WIFI_SSID  7
#define SCREEN_WIFI_PASS  8
#define SCREEN_ANNOUNCE   9
#define SCREEN_GPS        10
#define SCREEN_APPROACH   11
#define SCREEN_GPS_MATCH   12
#define SCREEN_TRIP_DETAIL 13
int currentScreen = SCREEN_MENU;

#define HEADER_H    42
#define ICON_SCALE   2

static int batFiltered = -1;

int readBatteryPercent() {
  const int  N = 32;
  static int s[N];
  // analogReadMilliVolts uses ESP32-S3 built-in ADC calibration — far more accurate
  for (int i = 0; i < N; i++) { s[i] = analogReadMilliVolts(BAT_ADC_PIN); delay(3); }
  for (int i = 0; i < N - 1; i++)
    for (int j = i + 1; j < N; j++)
      if (s[i] > s[j]) { int t = s[i]; s[i] = s[j]; s[j] = t; }
  long sum = 0;
  int  trim = N / 4;
  for (int i = trim; i < N - trim; i++) sum += s[i];
  int avgMv = (int)(sum / (N - 2 * trim));
  Serial.printf("[BAT] ADC pin mV=%d  (full=%d empty=%d)\n", avgMv, BAT_MV_FULL, BAT_MV_EMPTY);
  int pct = (int)((long)(avgMv - BAT_MV_EMPTY) * 100L / (BAT_MV_FULL - BAT_MV_EMPTY));
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  if (batFiltered < 0) batFiltered = pct;
  else                 batFiltered = (batFiltered * 3 + pct + 2) / 4;
  return batFiltered;
}

void pushImageScaled(int32_t x, int32_t y, int32_t w, int32_t h,
                     const uint16_t* img, uint8_t scale, uint16_t transparent) {
  for (int32_t row = 0; row < h; row++) {
    for (int32_t col = 0; col < w; col++) {
      uint16_t color = img[row * w + col];
      if (color == transparent) continue;
      tft.fillRect(x + col * scale, y + row * scale, scale, scale, color);
    }
  }
}

void drawSignalStatus() {
  tft.fillRect(40, 0, 120, HEADER_H - 1, TFT_BLACK);
  const char*    labels[] = { "No GPS", "GPS Weak", "GPS OK" };
  const uint16_t colors[] = { TFT_RED, TFT_ORANGE, TFT_GREEN };
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(colors[signalLevel], TFT_BLACK);
  tft.drawString(labels[signalLevel], 42, HEADER_H / 2);
}

void drawBatteryStatus() {
  tft.fillRect(349, 0, 131, HEADER_H - 1, TFT_BLACK);

  uint16_t col = (batteryPercent > 50) ? TFT_GREEN
               : (batteryPercent > 20) ? TFT_ORANGE
               :                         TFT_RED;

  // Battery body — 36×14 px, right-corner: [icon][nub] [% text]
  const int bw = 36, bh = 14;
  const int bx = 381, by = (HEADER_H - bh) / 2;

  // Body outline
  tft.drawRoundRect(bx, by, bw, bh, 3, TFT_WHITE);
  // Terminal nub
  tft.fillRect(bx + bw, by + 3, 5, bh - 6, TFT_WHITE);

  // Fill bar (3px inset → 30px max)
  int fillW = 30 * batteryPercent / 100;
  if (fillW > 0)
    tft.fillRect(bx + 3, by + 3, fillW, bh - 6, col);

  // % text — after nub, left-aligned (ends ~x=476)
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", batteryPercent);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(col, TFT_BLACK);
  tft.drawString(buf, bx + bw + 9, HEADER_H / 2);
}

void drawInternetSignal() {
  const int iconX    = 164;
  const int wifiScl  = 1;   // 16px tall — smaller than the other icons
  int iconY = (HEADER_H - wifiScl * WIFI_IMG_HEIGHT) / 2;
  tft.fillRect(iconX, 0, 86, HEADER_H - 1, TFT_BLACK);
  pushImageScaled(iconX, iconY, WIFI_IMG_WIDTH, WIFI_IMG_HEIGHT,
                  wifi_img, wifiScl, WIFI_IMG_TRANSPARENT);
  const char*    labels[] = { "---", "Low", "OK", "Hi", "Full" };
  const uint16_t colors[] = { TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_GREEN };
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  int dispSig = wifiConnected ? wifiSigLevel : 0;
  tft.setTextColor(colors[dispSig], TFT_BLACK);
  tft.drawString(labels[dispSig], iconX + wifiScl * WIFI_IMG_WIDTH + 4, HEADER_H / 2);
}

void drawSoundLevel() {
  const int iconX = 253;
  int iconY = (HEADER_H - ICON_SCALE * SOUND_IMG_HEIGHT) / 2;
  tft.fillRect(iconX, 0, 96, HEADER_H - 1, TFT_BLACK);
  pushImageScaled(iconX, iconY, SOUND_IMG_WIDTH, SOUND_IMG_HEIGHT,
                  sound_img, ICON_SCALE, SOUND_IMG_TRANSPARENT);
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", volumeLevel);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(buf, iconX + ICON_SCALE * SOUND_IMG_WIDTH + 4, HEADER_H / 2);
}

void drawHeader() {
  tft.fillRect(0, 0, 480, HEADER_H, TFT_BLACK);
  int iconY = (HEADER_H - ICON_SCALE * SATELLITE_HEIGHT) / 2;
  pushImageScaled(4, iconY, SATELLITE_WIDTH, SATELLITE_HEIGHT,
                  satellite_bmp, ICON_SCALE, 0xFFFF);
  drawSignalStatus();
  drawInternetSignal();
  drawSoundLevel();
  drawBatteryStatus();
  tft.drawFastHLine(0, HEADER_H - 1, 480, TFT_WHITE);
}

void updateSignal(int level)    { signalLevel    = level;   drawSignalStatus(); }
void updateBattery(int percent) { batteryPercent = percent; drawBatteryStatus(); }

// ── Menu ─────────────────────────────────────────────────────────────────────
#define BTN_SIZE       110
#define BTN_GAP         80
#define BTN_COLOR     0x4A69
#define BTN_BORDER_COL 0xAD55
#define BTN_BORDER_W     2
#define MENU_ICON_SCALE  2

struct MenuItem {
  const char*     label;
  const uint16_t* icon;
  int             iconW, iconH;
  uint16_t        transparent;
};

const MenuItem menuItems[] = {
  { "Trip",     route_img,    ROUTE_IMG_WIDTH,    ROUTE_IMG_HEIGHT,    ROUTE_IMG_TRANSPARENT    },
  { "Settings", settings_img, SETTINGS_IMG_WIDTH, SETTINGS_IMG_HEIGHT, SETTINGS_IMG_TRANSPARENT },
};
const int MENU_COUNT = sizeof(menuItems) / sizeof(menuItems[0]);

void drawMenuItem(int index) {
  int totalW = MENU_COUNT * BTN_SIZE + (MENU_COUNT - 1) * BTN_GAP;
  int startX = (480 - totalW) / 2;
  int cx  = startX + index * (BTN_SIZE + BTN_GAP) + BTN_SIZE / 2;
  int cy  = HEADER_H + (320 - HEADER_H) / 2;
  tft.fillCircle(cx, cy, BTN_SIZE / 2,                BTN_BORDER_COL);
  tft.fillCircle(cx, cy, BTN_SIZE / 2 - BTN_BORDER_W, BTN_COLOR);
  int scaledW = menuItems[index].iconW * MENU_ICON_SCALE;
  int scaledH = menuItems[index].iconH * MENU_ICON_SCALE;
  int iconX   = cx - scaledW / 2;
  int iconY   = cy - scaledH / 2 - 8;
  pushImageScaled(iconX, iconY, menuItems[index].iconW, menuItems[index].iconH,
                  menuItems[index].icon, MENU_ICON_SCALE, menuItems[index].transparent);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(menuItems[index].label, cx, cy + BTN_SIZE / 2 + 8);
}

void drawMenu() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  for (int i = 0; i < MENU_COUNT; i++) drawMenuItem(i);
}

int menuItemAt(int tx, int ty) {
  int totalW = MENU_COUNT * BTN_SIZE + (MENU_COUNT - 1) * BTN_GAP;
  int startX = (480 - totalW) / 2;
  int cy = HEADER_H + (320 - HEADER_H) / 2;
  for (int i = 0; i < MENU_COUNT; i++) {
    int cx = startX + i * (BTN_SIZE + BTN_GAP) + BTN_SIZE / 2;
    int r  = BTN_SIZE / 2;
    int dx = tx - cx, dy = ty - cy;
    if ((long)dx*dx + (long)dy*dy <= (long)r*r) return i;
  }
  return -1;
}

// ── Trip Screen ───────────────────────────────────────────────────────────────
#define SEARCH_X       8
#define SEARCH_Y      44
#define SEARCH_W     400
#define SEARCH_H      40
#define SEARCH_BTN_CX  456
#define SEARCH_BTN_CY  (SEARCH_Y + SEARCH_H / 2)
#define SEARCH_BTN_R    22

// Trip screen uses a shorter input box + professional rect button
#define TRIP_SEARCH_W  300
#define TRIP_BTN_X     316
#define TRIP_BTN_W     156

// Trip Detail screen — both buttons on the same row
#define TD_BTN_Y        BACK_BTN_Y   // same bottom row as back button
#define TD_BTN_H        BACK_BTN_H
#define TD_START_X      165
#define TD_START_W      130
#define TD_UPDATE_X     340
#define TD_UPDATE_W     130
#define TD_AUDIO_Y      168          // y for "Audio: X / Y files" label
#define TD_DL_AUDIO_X   140          // Download Audio button
#define TD_DL_AUDIO_Y   210
#define TD_DL_AUDIO_W   200
#define TD_DL_AUDIO_H    40

#define KB_Y         88
#define KB_KEY_W     46
#define KB_KEY_H     44
#define KB_KEY_GAP    2
#define KB_ROWS       4
#define KB_BS_W       66
#define KB_SP_W      140
#define KB_SHFT_W     70
#define KB_CLR_W      70
#define KB_BACK_W     80

#define KB_COL     0x2945
#define KB_SP_COL  0x39C7
#define KB_CLR_COL 0xA000
#define KB_BCK_COL 0x4208

char inputText[64] = "";
int  inputLen      = 0;
bool tripFound     = false;

const char* kbRowData[KB_ROWS] = {
  "1234567890",
  "QWERTYUIOP",
  "ASDFGHJKL",
  "ZXCVBNM"
};

const char* kbSymRows[] = {
  "!@#$%^&*()",
  "-_+=.,;:'\"",
  "/\\?<>~`|"
};
const int KB_SYM_ROWS = 3;

void drawKey(int x, int y, int w, int h, const char* label, uint16_t bg) {
  tft.fillRoundRect(x, y, w, h, 5, bg);
  tft.drawRoundRect(x, y, w, h, 5, 0x528A);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(label, x + w / 2, y + h / 2);
}

void drawKeyboard() {
  bool isAlpha = (kbMode == 4 || kbMode == 5);  // letters-only (no numbers row)
  bool isNum   = (kbMode == 3);
  bool isSym   = (kbMode == 2);
  bool isLower = (kbMode == 1 || kbMode == 5);
  int  rows    = isNum ? 1 : isAlpha ? (KB_ROWS - 1) : isSym ? KB_SYM_ROWS : KB_ROWS;
  // always clear the full keyboard area so switching modes cleans up
  tft.fillRect(0, KB_Y, 480, (KB_ROWS + 1) * (KB_KEY_H + KB_KEY_GAP), TFT_BLACK);
  for (int r = 0; r < rows; r++) {
    // alpha-only modes skip kbRowData[0] (numbers row) → start at index 1
    const char* rowStr = isSym ? kbSymRows[r] : isAlpha ? kbRowData[r + 1] : kbRowData[r];
    int len     = strlen(rowStr);
    int y       = KB_Y + r * (KB_KEY_H + KB_KEY_GAP);
    // don't append backspace to the numbers-only row — it makes the row too wide
    bool isLast = (r == rows - 1) && (kbMode != 3);
    int totalW = isLast
                 ? len * KB_KEY_W + len * KB_KEY_GAP + KB_BS_W
                 : len * KB_KEY_W + (len - 1) * KB_KEY_GAP;
    int startX = (480 - totalW) / 2;
    for (int c = 0; c < len; c++) {
      char ch = rowStr[c];
      char lbl[2] = { (isLower && ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch, '\0' };
      drawKey(startX + c * (KB_KEY_W + KB_KEY_GAP), y, KB_KEY_W, KB_KEY_H, lbl, KB_COL);
    }
    if (isLast) {
      int bsX = startX + len * (KB_KEY_W + KB_KEY_GAP);
      drawKey(bsX, y, KB_BS_W, KB_KEY_H, "<<", 0x4208);
    }
  }
  int spY      = KB_Y + rows * (KB_KEY_H + KB_KEY_GAP);
  int spTotalW = KB_BACK_W + KB_KEY_GAP + KB_SP_W + KB_KEY_GAP + KB_SHFT_W + KB_KEY_GAP + KB_CLR_W;
  int spStartX = (480 - spTotalW) / 2;
  int shftX    = spStartX + KB_BACK_W + KB_KEY_GAP + KB_SP_W + KB_KEY_GAP;
  int clrX2    = shftX + KB_SHFT_W + KB_KEY_GAP;
  // shift label: nums→ABC  UPPER→abc  lower→SYM  sym→123
  const char* shftLbl = (kbMode==3) ? "ABC" : (kbMode==2) ? "123" : (kbMode==0||kbMode==4) ? "abc" : "SYM";
  uint16_t    shftBg  = (kbMode==3) ? 0x0460 : (kbMode==2) ? 0x4A69 : (kbMode==0||kbMode==4) ? 0x4A69 : 0xC618;
  drawKey(spStartX,                           spY, KB_BACK_W, KB_KEY_H, "BACK",  KB_BCK_COL);
  drawKey(spStartX + KB_BACK_W + KB_KEY_GAP, spY, KB_SP_W,   KB_KEY_H, "SPACE", KB_SP_COL);
  drawKey(shftX,                              spY, KB_SHFT_W, KB_KEY_H, shftLbl, shftBg);
  drawKey(clrX2,                              spY, KB_CLR_W,  KB_KEY_H, "CLR",   KB_CLR_COL);
}

void updateSearchText() {
  tft.fillRect(SEARCH_X + 2, SEARCH_Y + 2, TRIP_SEARCH_W - 4, SEARCH_H - 4, TFT_WHITE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  if (inputLen > 0) {
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawString(inputText, SEARCH_X + 6, SEARCH_Y + SEARCH_H / 2);
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
    tft.drawString("Enter train number...", SEARCH_X + 6, SEARCH_Y + SEARCH_H / 2);
  }
}

// ── Settings Screen ───────────────────────────────────────────────────────────
#define SETT_BTN_H    40
#define SETT_BTN_GAP   4
#define SETT_START_Y  (HEADER_H + 8)
#define SETT_COUNT     6
#define SETT_PER_COL   5           // items per column before wrapping to next
#define SETT_COL1_X    8
#define SETT_COL2_X    244         // 8 + 228 + 8
#define SETT_COL_W     228

const char* settingLabels[] = {
  "Audio Volume",
  "Announcement",
  "Wifi Setting",
  "GPS Info",
  "Approach Dist",
  "GPS Match Dist"
};

void drawSettingsScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  for (int i = 0; i < SETT_COUNT; i++) {
    int col = i / SETT_PER_COL;
    int row = i % SETT_PER_COL;
    int x   = (col == 0) ? SETT_COL1_X : SETT_COL2_X;
    int y   = SETT_START_Y + row * (SETT_BTN_H + SETT_BTN_GAP);
    tft.fillRoundRect(x, y, SETT_COL_W, SETT_BTN_H, 8, 0x2945);
    tft.drawRoundRect(x, y, SETT_COL_W, SETT_BTN_H, 8, 0x4A69);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, 0x2945);
    tft.drawString(settingLabels[i], x + 10, y + SETT_BTN_H / 2);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TFT_DARKGREY, 0x2945);
    tft.drawString(">", x + SETT_COL_W - 8, y + SETT_BTN_H / 2);
  }
  drawBackButton();
}

bool settingButtonHit(int tx, int ty, int& idx) {
  for (int i = 0; i < SETT_COUNT; i++) {
    int col = i / SETT_PER_COL;
    int row = i % SETT_PER_COL;
    int x   = (col == 0) ? SETT_COL1_X : SETT_COL2_X;
    int y   = SETT_START_Y + row * (SETT_BTN_H + SETT_BTN_GAP);
    if (tx >= x && tx < x + SETT_COL_W &&
        ty >= y && ty < y + SETT_BTN_H) {
      idx = i; return true;
    }
  }
  return false;
}

// ── WiFi Persistence ──────────────────────────────────────────────────────────
void loadWifiNetworks() {
  wifiCount = prefs.getInt("wifiCount", 0);
  if (wifiCount > WIFI_MAX_NETS) wifiCount = WIFI_MAX_NETS;
  for (int i = 0; i < wifiCount; i++) {
    char sk[10], pk[10];
    snprintf(sk, sizeof(sk), "wssid%d", i);
    snprintf(pk, sizeof(pk), "wpass%d", i);
    prefs.getString(sk, wifiSSIDs[i], sizeof(wifiSSIDs[i]));
    prefs.getString(pk, wifiPasses[i], sizeof(wifiPasses[i]));
  }
}

void saveWifiNetworks() {
  prefs.putInt("wifiCount", wifiCount);
  for (int i = 0; i < wifiCount; i++) {
    char sk[10], pk[10];
    snprintf(sk, sizeof(sk), "wssid%d", i);
    snprintf(pk, sizeof(pk), "wpass%d", i);
    prefs.putString(sk, wifiSSIDs[i]);
    prefs.putString(pk, wifiPasses[i]);
  }
}

void addWifiNetwork(const char* ssid, const char* pass) {
  if (wifiCount >= WIFI_MAX_NETS) {
    for (int i = 0; i < WIFI_MAX_NETS - 1; i++) {
      strncpy(wifiSSIDs[i],  wifiSSIDs[i+1],  32); wifiSSIDs[i][32]  = '\0';
      strncpy(wifiPasses[i], wifiPasses[i+1], 64); wifiPasses[i][64] = '\0';
    }
    wifiCount = WIFI_MAX_NETS - 1;
  }
  strncpy(wifiSSIDs[wifiCount],  ssid, 32); wifiSSIDs[wifiCount][32]  = '\0';
  strncpy(wifiPasses[wifiCount], pass, 64); wifiPasses[wifiCount][64] = '\0';
  wifiCount++;
  saveWifiNetworks();
}

void connectWifi(int idx) {
  if (idx < 0 || idx >= wifiCount) return;
  WiFi.disconnect(true);
  delay(100);
  wifiConnected = false;
  wifiSigLevel  = 0;
  drawInternetSignal();
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSIDs[idx], wifiPasses[idx]);
}

// ── WiFi Screen ───────────────────────────────────────────────────────────────
#define WIFI_ROW_X      20
#define WIFI_ROW_W     390
#define WIFI_ROW_H      50
#define WIFI_ROW_GAP     6
#define WIFI_ROW_START  70

void drawWifiNetworkRow(int idx, int y) {
  bool isConn = wifiConnected && (strcmp(WiFi.SSID().c_str(), wifiSSIDs[idx]) == 0);
  uint16_t bg = isConn ? 0x0460 : 0x2945;
  tft.fillRoundRect(WIFI_ROW_X, y, WIFI_ROW_W, WIFI_ROW_H, 8, bg);
  tft.drawRoundRect(WIFI_ROW_X, y, WIFI_ROW_W, WIFI_ROW_H, 8, 0x4A69);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(wifiSSIDs[idx], WIFI_ROW_X + 12, y + WIFI_ROW_H / 2);
  if (isConn) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, bg);
    tft.drawString("Connected", WIFI_ROW_X + WIFI_ROW_W - 4, y + WIFI_ROW_H / 2);
  }
  tft.fillRoundRect(416, y + 4, 44, WIFI_ROW_H - 8, 6, 0xA000);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, 0xA000);
  tft.drawString("X", 438, y + WIFI_ROW_H / 2);
}

void drawWifiScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  if (wifiConnected) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    char msg[52];
    snprintf(msg, sizeof(msg), "Connected: %.36s", WiFi.SSID().c_str());
    tft.drawString(msg, 16, HEADER_H + 14);
  } else {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Not Connected", 16, HEADER_H + 14);
  }
  for (int i = 0; i < wifiCount; i++) {
    drawWifiNetworkRow(i, WIFI_ROW_START + i * (WIFI_ROW_H + WIFI_ROW_GAP));
  }
  if (wifiCount == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("No networks saved", 240, 160);
  }
  tft.fillRoundRect(20, 238, 440, 34, 8, 0x0460);
  tft.drawRoundRect(20, 238, 440, 34, 8, 0x07FF);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, 0x0460);
  tft.drawString("+ Add Network", 240, 255);
  drawBackButton();
}

// ── WiFi SSID / Password entry ────────────────────────────────────────────────
void updateWifiInput(bool isPass) {
  tft.fillRect(SEARCH_X + 2, SEARCH_Y + 2, SEARCH_W - 4, SEARCH_H - 4, TFT_WHITE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  if (wifiInputLen > 0) {
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    if (isPass) {
      char mask[65];
      memset(mask, '*', wifiInputLen);
      mask[wifiInputLen] = '\0';
      tft.drawString(mask, SEARCH_X + 6, SEARCH_Y + SEARCH_H / 2);
    } else {
      tft.drawString(wifiInputBuf, SEARCH_X + 6, SEARCH_Y + SEARCH_H / 2);
    }
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
    tft.drawString(isPass ? "Enter password..." : "Enter SSID...",
                   SEARCH_X + 6, SEARCH_Y + SEARCH_H / 2);
  }
}

static void drawWifiInputScreen(bool isPass) {
  kbMode = 0;
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.fillRoundRect(SEARCH_X, SEARCH_Y, SEARCH_W, SEARCH_H, 6, TFT_WHITE);
  tft.drawRoundRect(SEARCH_X, SEARCH_Y, SEARCH_W, SEARCH_H, 6, TFT_DARKGREY);
  tft.fillCircle(SEARCH_BTN_CX, SEARCH_BTN_CY, SEARCH_BTN_R, BTN_BORDER_COL);
  tft.fillCircle(SEARCH_BTN_CX, SEARCH_BTN_CY, SEARCH_BTN_R - 2, 0x0460);
  tft.setTextDatum(MC_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_WHITE, 0x0460);
  tft.drawString(isPass ? "SAVE" : "OK", SEARCH_BTN_CX, SEARCH_BTN_CY);
  updateWifiInput(isPass);
  drawKeyboard();
}
void drawWifiSSIDScreen() { drawWifiInputScreen(false); }
void drawWifiPassScreen() { drawWifiInputScreen(true);  }

// ── Shared keyboard helpers ───────────────────────────────────────────────────
static bool kbRowTouch(int tx, int ty, char* buf, int& len, int maxLen) {
  bool isAlpha = (kbMode == 4 || kbMode == 5);
  bool isSym   = (kbMode == 2);
  bool isLower = (kbMode == 1 || kbMode == 5);
  int  rows    = (kbMode==3) ? 1 : isAlpha ? (KB_ROWS-1) : isSym ? KB_SYM_ROWS : KB_ROWS;
  for (int r = 0; r < rows; r++) {
    int ry = KB_Y + r * (KB_KEY_H + KB_KEY_GAP);
    if (ty < ry || ty > ry + KB_KEY_H) continue;
    const char* row  = isSym ? kbSymRows[r] : isAlpha ? kbRowData[r+1] : kbRowData[r];
    int  rlen = strlen(row);
    bool last = (r == rows - 1) && (kbMode != 3);
    int  totW = last ? rlen*(KB_KEY_W+KB_KEY_GAP)+KB_BS_W : rlen*KB_KEY_W+(rlen-1)*KB_KEY_GAP;
    int  sx   = (480 - totW) / 2;
    for (int c = 0; c < rlen; c++) {
      int kx = sx + c*(KB_KEY_W+KB_KEY_GAP);
      if (tx >= kx && tx < kx + KB_KEY_W) {
        char ch = row[c];
        char t  = (isLower && ch>='A' && ch<='Z') ? (char)(ch+32) : ch;
        if (len < maxLen) { buf[len++] = t; buf[len] = '\0'; }
        return true;
      }
    }
    if (last) {
      int bx = sx + rlen*(KB_KEY_W+KB_KEY_GAP);
      if (tx >= bx && tx < bx + KB_BS_W) { if (len > 0) buf[--len] = '\0'; return true; }
    }
  }
  return false;
}

static int kbBottomTouch(int tx, int ty) {
  int rows = (kbMode==3) ? 1 : (kbMode==4||kbMode==5) ? (KB_ROWS-1) : (kbMode==2) ? KB_SYM_ROWS : KB_ROWS;
  int spY  = KB_Y + rows*(KB_KEY_H+KB_KEY_GAP);
  if (ty < spY || ty > spY + KB_KEY_H) return 0;
  const int W = KB_BACK_W+KB_KEY_GAP+KB_SP_W+KB_KEY_GAP+KB_SHFT_W+KB_KEY_GAP+KB_CLR_W;
  int sx = (480 - W) / 2;
  if (tx >= sx && tx < sx + KB_BACK_W)  return 1;  sx += KB_BACK_W  + KB_KEY_GAP;
  if (tx >= sx && tx < sx + KB_SP_W)    return 2;  sx += KB_SP_W    + KB_KEY_GAP;
  if (tx >= sx && tx < sx + KB_SHFT_W)  return 3;  sx += KB_SHFT_W  + KB_KEY_GAP;
  if (tx >= sx && tx < sx + KB_CLR_W)   return 4;
  return 0;
}

static void wifiKbInput(int tx, int ty, int maxLen, bool isPass) {
  if (kbRowTouch(tx, ty, wifiInputBuf, wifiInputLen, maxLen)) {
    updateWifiInput(isPass); return;
  }
  switch (kbBottomTouch(tx, ty)) {
    case 2:
      if (wifiInputLen < maxLen) { wifiInputBuf[wifiInputLen++] = ' '; wifiInputBuf[wifiInputLen] = '\0'; }
      updateWifiInput(isPass); break;
    case 3: kbMode = (kbMode+1)%3; drawKeyboard(); break;
    case 4: wifiInputLen = 0; wifiInputBuf[0] = '\0'; updateWifiInput(isPass); break;
  }
}

// ── WiFi Touch Handlers ───────────────────────────────────────────────────────
void handleWifiTouch(int tx, int ty) {
  if (backButtonHit(tx, ty)) {
    currentScreen = SCREEN_SETTINGS;
    drawSettingsScreen();
    return;
  }
  if (tx >= 20 && tx < 460 && ty >= 238 && ty < 272) {
    wifiInputLen = 0; wifiInputBuf[0] = '\0';
    currentScreen = SCREEN_WIFI_SSID;
    drawWifiSSIDScreen();
    return;
  }
  for (int i = 0; i < wifiCount; i++) {
    int y = WIFI_ROW_START + i * (WIFI_ROW_H + WIFI_ROW_GAP);
    if (ty < y || ty >= y + WIFI_ROW_H) continue;
    if (tx >= 20 && tx < 460) {
      if (tx >= 416) {
        for (int j = i; j < wifiCount - 1; j++) {
          strncpy(wifiSSIDs[j],  wifiSSIDs[j+1],  33);
          strncpy(wifiPasses[j], wifiPasses[j+1], 65);
        }
        wifiCount--;
        saveWifiNetworks();
      } else {
        connectWifi(i);
      }
      drawWifiScreen();
      return;
    }
  }
}

void handleWifiSSIDTouch(int tx, int ty) {
  int dx = tx - SEARCH_BTN_CX, dy = ty - SEARCH_BTN_CY;
  if (dx*dx + dy*dy <= SEARCH_BTN_R * SEARCH_BTN_R) {
    if (wifiInputLen > 0) {
      strncpy(pendingSSID, wifiInputBuf, 32); pendingSSID[32] = '\0';
      wifiInputLen = 0; wifiInputBuf[0] = '\0';
      currentScreen = SCREEN_WIFI_PASS;
      drawWifiPassScreen();
    }
    return;
  }
  if (kbBottomTouch(tx, ty) == 1) {
    currentScreen = SCREEN_WIFI;
    drawWifiScreen();
    return;
  }
  wifiKbInput(tx, ty, 32, false);
}

void handleWifiPassTouch(int tx, int ty) {
  int dx = tx - SEARCH_BTN_CX, dy = ty - SEARCH_BTN_CY;
  if (dx*dx + dy*dy <= SEARCH_BTN_R * SEARCH_BTN_R) {
    addWifiNetwork(pendingSSID, wifiInputBuf);
    connectWifi(wifiCount - 1);
    wifiInputLen = 0; wifiInputBuf[0] = '\0';
    currentScreen = SCREEN_WIFI;
    drawWifiScreen();
    return;
  }
  if (kbBottomTouch(tx, ty) == 1) {
    strncpy(wifiInputBuf, pendingSSID, 32); wifiInputBuf[32] = '\0';
    wifiInputLen = strlen(wifiInputBuf);
    currentScreen = SCREEN_WIFI_SSID;
    drawWifiSSIDScreen();
    updateWifiInput(false);
    return;
  }
  wifiKbInput(tx, ty, 64, true);
}

// ── Announcement Settings Screen ──────────────────────────────────────────────
#define ANN_ROW_H    48
#define ANN_ROW_X    16
#define ANN_ROW_W   448
#define ANN_ROW_BG  0x2945
#define ANN_R0_Y     50
#define ANN_R1_Y    106
#define ANN_R2_Y    162
#define ANN_R3_Y    218
#define ANN_BTN_W    58
#define ANN_BTN_H    42
#define ANN_BTN_YOF   6
#define ANN_MINUS_X 238
#define ANN_PLUS_X  378
#define ANN_VAL_X   300
#define ANN_VAL_W    74

static const int annRowY[4] = { ANN_R0_Y, ANN_R1_Y, ANN_R2_Y, ANN_R3_Y };
static const char* annLabels[4] = { "Far (m)", "Near (m)", "Repeats", "Gap (s)" };

void drawAnnounceRow(int row) {
  int ry = annRowY[row];
  tft.fillRoundRect(ANN_ROW_X, ry, ANN_ROW_W, ANN_ROW_H, 6, ANN_ROW_BG);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, ANN_ROW_BG);
  tft.drawString(annLabels[row], ANN_ROW_X + 14, ry + ANN_ROW_H / 2);
  tft.fillRoundRect(ANN_MINUS_X, ry + ANN_BTN_YOF, ANN_BTN_W, ANN_BTN_H, 6, 0x4A69);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, 0x4A69);
  tft.drawString("-", ANN_MINUS_X + ANN_BTN_W / 2, ry + ANN_BTN_YOF + ANN_BTN_H / 2);
  char val[10];
  if      (row == 0) snprintf(val, sizeof(val), "%d", (int)announceDistM);
  else if (row == 1) snprintf(val, sizeof(val), "%d", (int)announceNearDistM);
  else if (row == 2) snprintf(val, sizeof(val), "%d", announceReps);
  else               snprintf(val, sizeof(val), "%d", announceGapSec);
  tft.fillRect(ANN_VAL_X, ry, ANN_VAL_W, ANN_ROW_H, ANN_ROW_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, ANN_ROW_BG);
  tft.drawString(val, ANN_VAL_X + ANN_VAL_W / 2, ry + ANN_ROW_H / 2);
  tft.fillRoundRect(ANN_PLUS_X, ry + ANN_BTN_YOF, ANN_BTN_W, ANN_BTN_H, 6, 0x4A69);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, 0x4A69);
  tft.drawString("+", ANN_PLUS_X + ANN_BTN_W / 2, ry + ANN_BTN_YOF + ANN_BTN_H / 2);
}

void drawAnnounceScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Announcement Settings", 240, 43);
  for (int i = 0; i < 4; i++) drawAnnounceRow(i);
  drawBackButton();
}

void saveAnnounceSettings() {
  prefs.putFloat("annDist",     announceDistM);
  prefs.putFloat("annNearDist", announceNearDistM);
  prefs.putInt("annReps",      announceReps);
  prefs.putInt("annGapSec",    announceGapSec);
}

void handleAnnounceTouch(int tx, int ty) {
  if (backButtonHit(tx, ty)) {
    saveAnnounceSettings();
    currentScreen = SCREEN_SETTINGS;
    drawSettingsScreen();
    return;
  }
  for (int row = 0; row < 4; row++) {
    int ry  = annRowY[row];
    int by  = ry + ANN_BTN_YOF;
    int bye = by + ANN_BTN_H;
    if (ty < by || ty > bye) continue;
    bool isMinus = (tx >= ANN_MINUS_X && tx < ANN_MINUS_X + ANN_BTN_W);
    bool isPlus  = (tx >= ANN_PLUS_X  && tx < ANN_PLUS_X  + ANN_BTN_W);
    if (!isMinus && !isPlus) continue;
    int delta = isPlus ? 1 : -1;
    if (row == 0) {
      announceDistM += delta * 100.0f;
      if (announceDistM < 500.0f) announceDistM = 500.0f;
      if (announceDistM > 9000.0f)  announceDistM = 9000.0f;
      if (announceDistM > 0 && announceDistM <= announceNearDistM)
        announceNearDistM = announceDistM - 50.0f;
    } else if (row == 1) {
      announceNearDistM += delta * 50.0f;
      if (announceNearDistM < 150.0f) announceNearDistM = 150.0f;
      if (announceDistM > 0 && announceNearDistM >= announceDistM)
        announceNearDistM = announceDistM - 50.0f;
    } else if (row == 2) {
      announceReps += delta;
      if (announceReps < 1) announceReps = 1;
      if (announceReps > 8) announceReps = 8;
    } else {
      announceGapSec += delta * 5;
      if (announceGapSec <  5) announceGapSec = 5;
      if (announceGapSec > 60) announceGapSec = 60;
    }
    drawAnnounceRow(row);
    return;
  }
}

// ── Approach Distance Screen ─────────────────────────────────────────────────
#define APP_VAL_Y    (HEADER_H + 66)
#define APP_BTN_Y    (HEADER_H + 128)
#define APP_BTN_W    100
#define APP_BTN_H     52
#define APP_MINUS_X   90
#define APP_PLUS_X   290

void drawApproachValue() {
  tft.fillRect(80, APP_VAL_Y, 320, 54, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  char buf[14]; snprintf(buf, sizeof(buf), "%d m", (int)approachDistM);
  tft.drawString(buf, 240, APP_VAL_Y + 24);
}

void drawApproachScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2); tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Station Approach Dist", 240, HEADER_H + 18);
  drawApproachValue();
  // – button
  tft.fillRoundRect(APP_MINUS_X, APP_BTN_Y, APP_BTN_W, APP_BTN_H, 8, 0x4A69);
  tft.setTextSize(3); tft.setTextColor(TFT_WHITE, 0x4A69);
  tft.drawString("-", APP_MINUS_X + APP_BTN_W / 2, APP_BTN_Y + APP_BTN_H / 2);
  // + button
  tft.fillRoundRect(APP_PLUS_X, APP_BTN_Y, APP_BTN_W, APP_BTN_H, 8, 0x4A69);
  tft.drawString("+", APP_PLUS_X + APP_BTN_W / 2, APP_BTN_Y + APP_BTN_H / 2);
  tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Step: 100 m     Range: 100 - 2000 m", 240, APP_BTN_Y + APP_BTN_H + 16);
  tft.drawString("Distance at which next station is shown", 240, APP_BTN_Y + APP_BTN_H + 30);
  drawBackButton();
}

void handleApproachTouch(int tx, int ty) {
  if (backButtonHit(tx, ty)) {
    prefs.putFloat("approachDist", approachDistM);
    currentScreen = SCREEN_SETTINGS;
    drawSettingsScreen();
    return;
  }
  bool isMinus = (tx >= APP_MINUS_X && tx < APP_MINUS_X + APP_BTN_W &&
                  ty >= APP_BTN_Y   && ty < APP_BTN_Y   + APP_BTN_H);
  bool isPlus  = (tx >= APP_PLUS_X  && tx < APP_PLUS_X  + APP_BTN_W &&
                  ty >= APP_BTN_Y   && ty < APP_BTN_Y   + APP_BTN_H);
  if (!isMinus && !isPlus) return;
  approachDistM += (isPlus ? 100.0f : -100.0f);
  if (approachDistM < 100.0f)  approachDistM = 100.0f;
  if (approachDistM > 2000.0f) approachDistM = 2000.0f;
  drawApproachValue();
}

// ── GPS Match Distance Screen ─────────────────────────────────────────────────
// This distance is used ONCE at trip load: if the nearest station is within
// gpsMatchDistM metres, it becomes the current station; otherwise the nearest
// station becomes the NEXT station (and the one before it is current).
#define GMS_VAL_Y    (HEADER_H + 66)
#define GMS_BTN_Y    (HEADER_H + 128)
#define GMS_BTN_W    100
#define GMS_BTN_H     52
#define GMS_MINUS_X   90
#define GMS_PLUS_X   290

void drawGmsValue() {
  tft.fillRect(80, GMS_VAL_Y, 320, 54, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  char buf[14]; snprintf(buf, sizeof(buf), "%d m", (int)gpsMatchDistM);
  tft.drawString(buf, 240, GMS_VAL_Y + 24);
}

void drawGmsScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2); tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("GPS Start Match Dist", 240, HEADER_H + 18);
  drawGmsValue();
  tft.fillRoundRect(GMS_MINUS_X, GMS_BTN_Y, GMS_BTN_W, GMS_BTN_H, 8, 0x4A69);
  tft.setTextSize(3); tft.setTextColor(TFT_WHITE, 0x4A69);
  tft.drawString("-", GMS_MINUS_X + GMS_BTN_W / 2, GMS_BTN_Y + GMS_BTN_H / 2);
  tft.fillRoundRect(GMS_PLUS_X, GMS_BTN_Y, GMS_BTN_W, GMS_BTN_H, 8, 0x4A69);
  tft.drawString("+", GMS_PLUS_X + GMS_BTN_W / 2, GMS_BTN_Y + GMS_BTN_H / 2);
  tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Step: 50 m     Range: 50 - 1000 m", 240, GMS_BTN_Y + GMS_BTN_H + 16);
  tft.drawString("Nearest station <= this -> set as CURRENT on trip load", 240, GMS_BTN_Y + GMS_BTN_H + 30);
  drawBackButton();
}

void handleGmsTouch(int tx, int ty) {
  if (backButtonHit(tx, ty)) {
    prefs.putFloat("gpsMatchDist", gpsMatchDistM);
    currentScreen = SCREEN_SETTINGS;
    drawSettingsScreen();
    return;
  }
  bool isMinus = (tx >= GMS_MINUS_X && tx < GMS_MINUS_X + GMS_BTN_W &&
                  ty >= GMS_BTN_Y   && ty < GMS_BTN_Y   + GMS_BTN_H);
  bool isPlus  = (tx >= GMS_PLUS_X  && tx < GMS_PLUS_X  + GMS_BTN_W &&
                  ty >= GMS_BTN_Y   && ty < GMS_BTN_Y   + GMS_BTN_H);
  if (!isMinus && !isPlus) return;
  gpsMatchDistM += (isPlus ? 50.0f : -50.0f);
  if (gpsMatchDistM <   50.0f) gpsMatchDistM =   50.0f;
  if (gpsMatchDistM > 1000.0f) gpsMatchDistM = 1000.0f;
  drawGmsValue();
}

// ── Volume Screen ─────────────────────────────────────────────────────────────
#define VOL_BAR_X    40
#define VOL_BAR_Y   170
#define VOL_BAR_W   400
#define VOL_BAR_H    12
#define VOL_THUMB_R  16
#define VOL_MINUS_X  60
#define VOL_MINUS_Y 220
#define VOL_MINUS_W  80
#define VOL_MINUS_H  46
#define VOL_PLUS_X  340
#define VOL_PLUS_Y  220
#define VOL_PLUS_W   80
#define VOL_PLUS_H   46

void drawVolumeSlider() {
  tft.fillRect(0, HEADER_H + 1, 480, 320 - HEADER_H - 1, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Audio Volume", 240, 90);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", volumeLevel);
  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(buf, 240, 130);
  tft.fillRoundRect(VOL_BAR_X, VOL_BAR_Y, VOL_BAR_W, VOL_BAR_H, 6, 0x2945);
  int fillW = volumeLevel * VOL_BAR_W / 100;
  if (fillW > 0)
    tft.fillRoundRect(VOL_BAR_X, VOL_BAR_Y, fillW, VOL_BAR_H, 6, TFT_GREEN);
  int thumbX = VOL_BAR_X + fillW;
  int thumbY = VOL_BAR_Y + VOL_BAR_H / 2;
  tft.fillCircle(thumbX, thumbY, VOL_THUMB_R,     TFT_WHITE);
  tft.drawCircle(thumbX, thumbY, VOL_THUMB_R,     TFT_GREEN);
  tft.drawCircle(thumbX, thumbY, VOL_THUMB_R - 1, TFT_GREEN);
  tft.fillRoundRect(VOL_MINUS_X, VOL_MINUS_Y, VOL_MINUS_W, VOL_MINUS_H, 8, 0x4A69);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, 0x4A69);
  tft.drawString("-", VOL_MINUS_X + VOL_MINUS_W / 2, VOL_MINUS_Y + VOL_MINUS_H / 2);
  tft.fillRoundRect(VOL_PLUS_X, VOL_PLUS_Y, VOL_PLUS_W, VOL_PLUS_H, 8, 0x4A69);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, 0x4A69);
  tft.drawString("+", VOL_PLUS_X + VOL_PLUS_W / 2, VOL_PLUS_Y + VOL_PLUS_H / 2);
  drawBackButton();
}

void drawVolumeScreen() {
  tft.fillRect(0, 0, 480, 320, TFT_BLACK);
  drawHeader();
  drawVolumeSlider();
}

void applyVolume(int v) {
  if (v < 0)   v = 0;
  if (v > 100) v = 100;
  volumeLevel = v;
  prefs.putInt("volume", volumeLevel);
  drawVolumeSlider();
  drawSoundLevel();
}

// ── Trip Screen draw ──────────────────────────────────────────────────────────
void drawTripScreen() {
  kbMode = 3;
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.fillRoundRect(SEARCH_X, SEARCH_Y, TRIP_SEARCH_W, SEARCH_H, 6, TFT_WHITE);
  tft.drawRoundRect(SEARCH_X, SEARCH_Y, TRIP_SEARCH_W, SEARCH_H, 6, TFT_DARKGREY);
  tft.fillRoundRect(TRIP_BTN_X, SEARCH_Y, TRIP_BTN_W, SEARCH_H, 6, 0x0460);
  tft.drawRoundRect(TRIP_BTN_X, SEARCH_Y, TRIP_BTN_W, SEARCH_H, 6, 0x07FF);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, 0x0460);
  tft.drawString("SEARCH", TRIP_BTN_X + TRIP_BTN_W / 2, SEARCH_Y + SEARCH_H / 2);
  updateSearchText();
  drawKeyboard();
}

// ── Station State ─────────────────────────────────────────────────────────────

// Sliding 3-slot window: [0]=just passed, [1]=NEXT (displayed), [2]=UPCOMING (displayed)
static StaEntry staWin[3];
static int      totalSeqCount   = 0;   // total stations in the file
static int      audioFilesFound = 0;   // cached count of existing .wav files
static char     tripFilePath[80] = "";
char            tripName[32] = "";

inline bool hasNext1() { return staWin[1].seqNo > 0; }
inline bool hasNext2() { return staWin[2].seqNo > 0; }

// Count how many station .wav files already exist on SD for the current trip.
static int countAudioFiles() {
  if (tripFilePath[0] == '\0') return 0;
  SPI.end(); SPI.begin(12, SD_MISO, 11, SD_CS);
  if (!SD.begin(SD_CS)) { SPI.end(); SPI.begin(12, TOUCH_MISO, 11); return 0; }
  File f = SD.open(tripFilePath);
  if (!f) { SD.end(); SPI.end(); SPI.begin(12, TOUCH_MISO, 11); return 0; }
  JsonDocument filter; filter[0]["code"] = true;
  JsonDocument doc;
  deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  int count = 0;
  for (JsonObject st : doc.as<JsonArray>()) {
    const char* code = st["code"] | "";
    if (code[0] == '\0') continue;
    char path[20]; snprintf(path, sizeof(path), "/%s.wav", code);
    if (SD.exists(path)) count++;
  }
  SD.end(); SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
  return count;
}

// Load one station by seqNo from SD into dst. Returns false if not found.
static bool loadOneStation(int seqNo, StaEntry& dst) {
  dst.seqNo = -1;
  if (seqNo < 1 || seqNo > totalSeqCount || tripFilePath[0] == '\0') return false;

  // Step 1: connect SD card
  SPI.end();
  SPI.begin(12, SD_MISO, 11, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] loadOneStation: SD.begin() failed");
    SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    return false;
  }
  Serial.printf("[SD] loadOneStation: connected (heap=%d)\n", ESP.getFreeHeap());

  // Step 2: open file
  File f = SD.open(tripFilePath);
  if (!f) {
    Serial.printf("[SD] loadOneStation: open failed -> %s\n", tripFilePath);
    SD.end(); digitalWrite(SD_CS, HIGH);
    SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    return false;
  }

  // Step 3: parse
  JsonDocument filter;
  filter[0]["seqNo"]  = true;  filter[0]["name"]   = true;
  filter[0]["lat"]    = true;  filter[0]["lon"]    = true;
  filter[0]["code"]   = true;  filter[0]["engUrl"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  SD.end();
  digitalWrite(SD_CS, HIGH);
  SPI.end(); SPI.begin(12, TOUCH_MISO, 11);

  if (err) {
    Serial.printf("[SD] loadOneStation: parse error -> %s\n", err.c_str());
    return false;
  }

  for (JsonObject st : doc.as<JsonArray>()) {
    if ((st["seqNo"] | 0) == seqNo) {
      dst.seqNo = seqNo;
      strncpy(dst.name, st["name"] | "?", 63); dst.name[63] = '\0';
      dst.lat = st["lat"] | 0.0f;
      dst.lon = st["lon"] | 0.0f;
      strncpy(dst.code,   st["code"]   | "", 11);  dst.code[11]   = '\0';
      strncpy(dst.engUrl, st["engUrl"] | "", 191); dst.engUrl[191] = '\0';
      return true;
    }
  }
  return false;
}

static bool          pendingAdvance = false;
static unsigned long lastAdvanceMs  = 0;

// Announcement state
static bool          annActive       = false;   // sequence in progress
static int           annFired        = 0;        // how many announcements fired so far
static unsigned long annLastMs       = 0;        // time of last announcement
static unsigned long annBannerMs     = 0;        // time banner was drawn (0 = none)
static uint8_t       annOutsideCount = 0;        // consecutive GPS readings outside Far zone

void advanceStation() {
  if (!hasNext1()) return;
  if (millis() - lastAdvanceMs < 8000UL) return;  // guard: min 8 s between advances
  lastAdvanceMs = millis();
  // Slide: [1]→[0], [2]→[1], mark [2] empty
  staWin[0] = staWin[1];
  staWin[1] = staWin[2];
  staWin[2].seqNo = -1;
  pendingAdvance = true;  // defer SD load to main loop — never block GPS serial here
  annActive = false; annFired = 0; annBannerMs = 0; annOutsideCount = 0;   // reset for the new next station
  Serial.printf("[STA] Advance queued → next=%s\n", hasNext1() ? staWin[1].name : "end");
}

bool searchTrip(const char* trainNum) {
  if (!sdReady) {
    Serial.println("[SD] ERROR: SD not initialised — check card at boot");
    return false;
  }

  // Step 1: connect SD card
  SPI.end();
  SPI.begin(12, SD_MISO, 11, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] searchTrip: SD.begin() failed");
    SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    return false;
  }
  Serial.printf("[SD] searchTrip: connected (heap=%d)\n", ESP.getFreeHeap());

  // Step 2: open file
  snprintf(tripFilePath, sizeof(tripFilePath), "/%s.json", trainNum);
  File f = SD.open(tripFilePath);
  if (!f) {
    snprintf(tripFilePath, sizeof(tripFilePath), "%s.json", trainNum);
    f = SD.open(tripFilePath);
  }
  if (!f) {
    Serial.printf("[SD] ERROR: File not found -> %s\n", tripFilePath);
    tripFilePath[0] = '\0';
    SD.end(); digitalWrite(SD_CS, HIGH);
    SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    return false;
  }
  Serial.printf("[SD] Opened: %s  size=%u bytes\n", tripFilePath, (unsigned)f.size());

  // Step 3: parse
  JsonDocument filter;
  filter[0]["seqNo"]  = true;  filter[0]["name"]   = true;
  filter[0]["lat"]    = true;  filter[0]["lon"]    = true;
  filter[0]["code"]   = true;  filter[0]["engUrl"] = true;

  JsonDocument doc;  // temporary — freed at end of function
  DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  SD.end();
  digitalWrite(SD_CS, HIGH);
  SPI.end(); SPI.begin(12, TOUCH_MISO, 11);

  if (err) { Serial.printf("[SD] JSON parse failed -> %s\n", err.c_str()); return false; }

  JsonArray arr = doc.as<JsonArray>();
  totalSeqCount = (int)arr.size();
  if (totalSeqCount == 0) { Serial.println("[SD] ERROR: JSON array empty"); return false; }

  // Build index array sorted by seqNo
  static int sortIdx[300];
  int n = totalSeqCount < 300 ? totalSeqCount : 300;
  for (int i = 0; i < n; i++) sortIdx[i] = i;
  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if ((arr[sortIdx[i]]["seqNo"] | 0) > (arr[sortIdx[j]]["seqNo"] | 0)) {
        int t = sortIdx[i]; sortIdx[i] = sortIdx[j]; sortIdx[j] = t;
      }

  // Find nearest station by GPS
  int winStart = 0;
  if (gpsHasFix) {
    int   nearIdx   = 0;
    float nearDistM = haversineKm(gpsLat, gpsLon,
                        arr[sortIdx[0]]["lat"] | 0.0f,
                        arr[sortIdx[0]]["lon"] | 0.0f) * 1000.0f;
    for (int i = 1; i < n; i++) {
      float d = haversineKm(gpsLat, gpsLon,
                  arr[sortIdx[i]]["lat"] | 0.0f,
                  arr[sortIdx[i]]["lon"] | 0.0f) * 1000.0f;
      if (d < nearDistM) { nearDistM = d; nearIdx = i; }
    }
    // Within gpsMatchDistM → GPS is AT that station (it becomes current, +1 becomes next).
    // Outside it           → GPS is between stations (nearest becomes next, -1 becomes current).
    winStart = (nearDistM < gpsMatchDistM) ? nearIdx : (nearIdx > 0 ? nearIdx - 1 : 0);
    Serial.printf("[SD] Nearest=%d dist=%.0fm matchDist=%.0f winStart=%d\n",
                  nearIdx, nearDistM, gpsMatchDistM, winStart);
  }

  // Fill 3-slot window — only these 3 stay in RAM
  auto fill = [&](StaEntry& e, int pos) {
    if (pos < 0 || pos >= n) { e.seqNo = -1; return; }
    JsonObject st = arr[sortIdx[pos]];
    e.seqNo = st["seqNo"] | 0;
    strncpy(e.name, st["name"] | "?", 63); e.name[63] = '\0';
    e.lat = st["lat"] | 0.0f;
    e.lon = st["lon"] | 0.0f;
    strncpy(e.code,   st["code"]   | "", 11);  e.code[11]   = '\0';
    strncpy(e.engUrl, st["engUrl"] | "", 191); e.engUrl[191] = '\0';
  };
  fill(staWin[0], winStart);
  fill(staWin[1], winStart + 1);
  fill(staWin[2], winStart + 2);

  strncpy(tripName, trainNum, sizeof(tripName) - 1);
  tripName[sizeof(tripName) - 1] = '\0';

  Serial.printf("[SD] %d stations  window:[%s][%s][%s]\n", totalSeqCount,
                staWin[0].seqNo > 0 ? staWin[0].name : "-",
                staWin[1].seqNo > 0 ? staWin[1].name : "-",
                staWin[2].seqNo > 0 ? staWin[2].name : "-");
  audioFilesFound = countAudioFiles();   // cache audio count for trip detail display
  return true;  // doc freed here — only staWin[3] (270 bytes) kept
}

// ── Shared Back Button ────────────────────────────────────────────────────────
#define BACK_BTN_X   10
#define BACK_BTN_Y  272
#define BACK_BTN_W  110
#define BACK_BTN_H   36

// Station-screen PLAY test button (right side of same row as BACK)
#define PLAY_BTN_X  360
#define PLAY_BTN_Y  272
#define PLAY_BTN_W  110
#define PLAY_BTN_H   36

// Not-Found screen DOWNLOAD button (right side of same row as BACK)
#define DL_BTN_X   310
#define DL_BTN_Y   BACK_BTN_Y
#define DL_BTN_W   162
#define DL_BTN_H   BACK_BTN_H

void drawBackButton() {
  tft.fillRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 6, 0x2104);
  tft.drawRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 6, TFT_WHITE);
  int cy = BACK_BTN_Y + BACK_BTN_H / 2;
  // Left-arrow triangle
  int ax = BACK_BTN_X + 14;
  tft.fillTriangle(ax, cy, ax + 10, cy - 7, ax + 10, cy + 7, TFT_WHITE);
  // BACK label
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, 0x2104);
  tft.drawString("BACK", ax + 14, cy);
}

bool backButtonHit(int tx, int ty) {
  return tx >= BACK_BTN_X && tx < BACK_BTN_X + BACK_BTN_W &&
         ty >= BACK_BTN_Y && ty < BACK_BTN_Y + BACK_BTN_H;
}

void drawStationPlayButton() {
  tft.fillRoundRect(PLAY_BTN_X, PLAY_BTN_Y, PLAY_BTN_W, PLAY_BTN_H, 6, 0x07E0);
  tft.drawRoundRect(PLAY_BTN_X, PLAY_BTN_Y, PLAY_BTN_W, PLAY_BTN_H, 6, TFT_WHITE);
  int cy = PLAY_BTN_Y + PLAY_BTN_H / 2;
  // Play triangle
  int tx = PLAY_BTN_X + 16;
  tft.fillTriangle(tx, cy - 8, tx, cy + 8, tx + 12, cy, TFT_BLACK);
  // PLAY label
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK, 0x07E0);
  tft.drawString("PLAY", tx + 16, cy);
}

bool stationPlayButtonHit(int tx, int ty) {
  return tx >= PLAY_BTN_X && tx < PLAY_BTN_X + PLAY_BTN_W &&
         ty >= PLAY_BTN_Y && ty < PLAY_BTN_Y + PLAY_BTN_H;
}

// ── Trip Detail Screen ────────────────────────────────────────────────────────
void drawTripDetailScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Trip Details", 16, 58);
  if (tripFound) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(inputText, 240, 92);
    char staBuf[24];
    snprintf(staBuf, sizeof(staBuf), "Stations: %d", totalSeqCount);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(staBuf, 240, 126);
    // Audio file count
    char audBuf[32];
    snprintf(audBuf, sizeof(audBuf), "Audio: %d / %d files", audioFilesFound, totalSeqCount);
    tft.setTextSize(2);
    tft.setTextColor(audioFilesFound == totalSeqCount ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    tft.drawString(audBuf, 240, TD_AUDIO_Y);
    tft.setTextDatum(MC_DATUM);
    tft.fillRoundRect(TD_START_X, TD_BTN_Y, TD_START_W, TD_BTN_H, 8, 0x07E0);
    tft.drawRoundRect(TD_START_X, TD_BTN_Y, TD_START_W, TD_BTN_H, 8, TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, 0x07E0);
    tft.drawString("START", TD_START_X + TD_START_W / 2, TD_BTN_Y + TD_BTN_H / 2);
    tft.fillRoundRect(TD_UPDATE_X, TD_BTN_Y, TD_UPDATE_W, TD_BTN_H, 8, 0x0460);
    tft.drawRoundRect(TD_UPDATE_X, TD_BTN_Y, TD_UPDATE_W, TD_BTN_H, 8, 0x07FF);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, 0x0460);
    tft.drawString("UPDATE", TD_UPDATE_X + TD_UPDATE_W / 2, TD_BTN_Y + TD_BTN_H / 2);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(inputText, 240, 87);
    tft.fillCircle(240, 152, 38, 0x6000);
    tft.setTextSize(3);
    tft.setTextColor(TFT_RED, 0x6000);
    tft.drawString("X", 240, 152);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Trip Not Found", 240, 204);
    tft.fillRoundRect(DL_BTN_X, DL_BTN_Y, DL_BTN_W, DL_BTN_H, 6, 0x0460);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, 0x0460);
    tft.drawString("DOWNLOAD", DL_BTN_X + DL_BTN_W / 2, DL_BTN_Y + DL_BTN_H / 2);
  }
  drawBackButton();
}

// ── Download trip JSON from API and save to SD ────────────────────────────────
static void downloadAndSaveTrip(const char* trainNum) {
  // Full-screen progress display
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2); tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Updating Trip", 240, HEADER_H + 28);

  auto showLine = [](const char* msg, uint16_t col) {
    tft.fillRect(0, HEADER_H + 48, 480, 28, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); tft.setTextSize(2);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(msg, 240, HEADER_H + 62);
  };

  if (!wifiConnected) {
    showLine("No WiFi!", TFT_RED);
    delay(1500); drawTripDetailScreen(); return;
  }

  // ── Step 1: Download JSON ─────────────────────────────────────────────
  showLine("Downloading JSON...", TFT_YELLOW);

  char url[128];
  snprintf(url, sizeof(url),
    "https://api.mindcoinapps.com/sas_mas_api/routes/downloadStationsByRouteNameJson/%s",
    trainNum);

  WiFiClientSecure wclient; wclient.setInsecure();
  HTTPClient http;
  http.begin(wclient, url); http.setTimeout(15000);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    char msg[32]; snprintf(msg, sizeof(msg), "HTTP Err: %d", httpCode);
    showLine(msg, TFT_RED); delay(1500); drawTripDetailScreen(); return;
  }

  char filePath[80];
  snprintf(filePath, sizeof(filePath), "/%s.json", trainNum);
  bool jsonSaved = false;

  {
    String payload = http.getString();
    http.end();
    if (payload.length() < 2) {
      showLine("Empty response", TFT_RED); delay(1500); drawTripDetailScreen(); return;
    }

    // ── Step 2: Save JSON to SD ───────────────────────────────────────
    showLine("Saving JSON...", TFT_YELLOW);
    SPI.end(); SPI.begin(12, SD_MISO, 11, SD_CS);
    if (!SD.begin(SD_CS)) {
      SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
      showLine("SD Error", TFT_RED); delay(1500); drawTripDetailScreen(); return;
    }
    SD.remove(filePath);
    File f = SD.open(filePath, FILE_WRITE);
    if (f) { f.write((const uint8_t*)payload.c_str(), payload.length()); f.close(); jsonSaved = true; }
    // Keep SD active for audio downloads below
  }  // payload freed here

  if (!jsonSaved) {
    SD.end(); SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    showLine("Save Failed", TFT_RED); delay(1500); drawTripDetailScreen(); return;
  }

  // ── Step 3: Parse saved JSON for station audio ────────────────────────
  showLine("Reading stations...", TFT_CYAN);
  File jf = SD.open(filePath);
  if (!jf) {
    SD.end(); SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    showLine("Read Error", TFT_RED); delay(1500); drawTripDetailScreen(); return;
  }
  JsonDocument filter; filter[0]["code"] = true; filter[0]["engUrl"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jf, DeserializationOption::Filter(filter));
  jf.close();
  if (err) {
    SD.end(); SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
    showLine("JSON Parse Error", TFT_RED); delay(1500); drawTripDetailScreen(); return;
  }

  JsonArray arr = doc.as<JsonArray>();
  int stationTotal = (int)arr.size();

  // ── Step 4: Download audio files (overwrite if exist) ─────────────────
  if (stationTotal > 0 && WiFi.status() == WL_CONNECTED) {
    showLine("Downloading audio...", TFT_CYAN);
    int done = 0;
    for (JsonObject st : arr) {
      const char* code = st["code"]   | "";
      const char* aurl = st["engUrl"] | "";
      if (code[0] == '\0') continue;
      done++;
      char wavPath[20]; snprintf(wavPath, sizeof(wavPath), "/%s.wav", code);
      // Progress: large "X / N" with station code below
      tft.fillRect(0, HEADER_H + 90, 480, 130, TFT_BLACK);
      tft.setTextSize(4); tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      char prog[20]; snprintf(prog, sizeof(prog), "%d / %d", done, stationTotal);
      tft.drawString(prog, 240, HEADER_H + 140);
      tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(code, 240, HEADER_H + 185);
      if (aurl[0] == '\0') { Serial.printf("[UPD-AUD] %s: no engUrl\n", code); continue; }
      if (SD.exists(wavPath)) SD.remove(wavPath);
      downloadAudioToSD(aurl, wavPath);   // SD already initialised
    }
  }

  SD.end(); SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
  audioFilesFound = countAudioFiles();

  // ── Done ──────────────────────────────────────────────────────────────
  tft.fillRect(0, HEADER_H + 90, 480, 130, TFT_BLACK);
  tft.setTextSize(2); tft.setTextColor(TFT_GREEN, TFT_BLACK);
  char doneMsg[48];
  snprintf(doneMsg, sizeof(doneMsg), "Done!  Audio: %d / %d", audioFilesFound, stationTotal);
  tft.drawString(doneMsg, 240, HEADER_H + 150);
  delay(1000);

  tripFound = searchTrip(trainNum);
  currentScreen = SCREEN_TRIP_DETAIL;
  drawTripDetailScreen();
}

// ── Not-Found Screen ──────────────────────────────────────────────────────────
void drawNotFoundScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.fillCircle(240, 152, 45, 0x6000);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_RED, 0x6000);
  tft.drawString("X", 240, 152);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Trip Not Found", 240, 208);
  tft.setTextSize(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  char msg[20];
  snprintf(msg, sizeof(msg), "Trip: %s", inputText);
  tft.drawString(msg, 240, 232);
  drawBackButton();
  // Download button
  tft.fillRoundRect(DL_BTN_X, DL_BTN_Y, DL_BTN_W, DL_BTN_H, 6, 0x0460);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, 0x0460);
  tft.drawString("DOWNLOAD", DL_BTN_X + DL_BTN_W / 2, DL_BTN_Y + DL_BTN_H / 2);
}

// ── Station Screen ────────────────────────────────────────────────────────────
// Layout — header row: y=42..92  content: y=92..320
#define STA_HDR_Y    (HEADER_H + 25)   // trip name + speed vertical centre (in button row)
#define STA_DIV1_Y   (HEADER_H + 50)   // BOX1 top reference

// Back and Play buttons — bottom of screen
#define STA_BACK_X    10
#define STA_BACK_Y    278
#define STA_BACK_W    80
#define STA_BACK_H    34
#define STA_PLAY_X    390
#define STA_PLAY_Y    278
#define STA_PLAY_W    80
#define STA_PLAY_H    34
#define STA_NAME1_Y  (HEADER_H + 78)   // next station name     (top-centre, size 5 = 40px)
#define STA_DIST1_Y  (HEADER_H + 120)  // next station distance (size 3 = 24px)
#define STA_DIV2_Y   (HEADER_H + 162)  // BOX2 divider reference
#define STA_NAME2_Y  (HEADER_H + 168)  // upcoming station name (size 4 = 32px)
#define STA_DIST2_Y  (HEADER_H + 202)  // upcoming distance     (size 3 = 24px)
#define STA_SPEED_Y  (HEADER_H + 178)  // (unused — speed is in header row)

// ── Announcement System — all values are configurable ─────────────────────
#define ANN_TRIGGER_DIST_M   500.0f   // start announcing when within this distance (m)
#define ANN_TOTAL_COUNT      4         // total announcements per station approach
#define ANN_INTERVAL_MS      10000UL   // gap between announcements (ms)
#define ANN_BANNER_MS        3000UL    // how long the on-screen banner stays visible (ms)

// MAX98357A I2S pins — BCK=35, LRC=36, DIN=37
// MAX98357A SD pin → 3.3V (or float) to enable amp; GAIN → float = 9 dB
#define ANN_I2S_BCK_PIN   35   // MAX98357A BCLK
#define ANN_I2S_WS_PIN    36   // MAX98357A LRC
#define ANN_I2S_DOUT_PIN  37   // MAX98357A DIN
#define AMP_RELAY_PIN     38   // Relay IN — HIGH = amp ON, LOW = amp OFF (power saving)

// Redraw everything below the first divider
void drawStationContent() {
  annBannerMs = 0;   // full redraw wipes any banner — clear the timer
  tft.fillRect(0, STA_DIV1_Y - 4, 480, STA_DIST2_Y + 32 - STA_DIV1_Y, TFT_BLACK);

  // ── Bordered box — next station (white border, 2px thick) ─────────────────
  for (int t = 0; t < 2; t++)
    tft.drawRoundRect(4 + t, STA_DIV1_Y - 3 + t,
                      472 - 2*t, STA_DIV2_Y - STA_DIV1_Y - 2 - 2*t, 8, TFT_WHITE);

  tft.setTextDatum(TC_DATUM);

  // ── Next station name (size 5) — orange while announcing, white otherwise ──
  if (hasNext1()) {
    uint16_t name1Col = annActive ? TFT_ORANGE : TFT_WHITE;
    tft.setTextSize(5); tft.setTextColor(name1Col, TFT_BLACK);
    tft.drawString(staWin[1].name, 240, STA_NAME1_Y);
    tft.setTextSize(3); tft.setTextColor(TFT_GREEN, TFT_BLACK);
    if (gpsHasFix) {
      float d = haversineKm(gpsLat, gpsLon, staWin[1].lat, staWin[1].lon) * 1000.0f;
      char buf[24]; snprintf(buf, sizeof(buf), "%.0f m", d);
      tft.drawString(buf, 240, STA_DIST1_Y);
    } else {
      tft.drawString("-- m", 240, STA_DIST1_Y);
    }
  } else {
    tft.setTextSize(5); tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("End of Route", 240, STA_NAME1_Y);
  }

  // ── Upcoming station name (size 5) ────────────────────────────────────────
  if (hasNext2()) {
    tft.setTextSize(4); tft.setTextColor(0xC618, TFT_BLACK);
    tft.drawString(staWin[2].name, 240, STA_NAME2_Y);
    tft.setTextSize(3); tft.setTextColor(0x07FF, TFT_BLACK);
    if (gpsHasFix) {
      float d = haversineKm(gpsLat, gpsLon, staWin[2].lat, staWin[2].lon) * 1000.0f;
      char buf[24]; snprintf(buf, sizeof(buf), "%.0f m", d);
      tft.drawString(buf, 240, STA_DIST2_Y);
    } else {
      tft.drawString("-- m", 240, STA_DIST2_Y);
    }
  } else if (hasNext1()) {
    tft.setTextSize(4); tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Last Station", 240, STA_NAME2_Y);
  }
}

// Lightweight refresh: repaint both distance numbers
void updateDistancesDisplay() {
  tft.setTextDatum(TC_DATUM);

  // Next station distance (size 3, 24px)
  tft.fillRect(0, STA_DIST1_Y, 480, 24, TFT_BLACK);
  tft.setTextSize(3); tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if (hasNext1() && gpsHasFix) {
    float d = haversineKm(gpsLat, gpsLon, staWin[1].lat, staWin[1].lon) * 1000.0f;
    char buf[24]; snprintf(buf, sizeof(buf), "%.0f m", d);
    tft.drawString(buf, 240, STA_DIST1_Y);
  } else {
    tft.drawString("-- m", 240, STA_DIST1_Y);
  }

  // Upcoming distance (size 3, 24px)
  tft.fillRect(0, STA_DIST2_Y, 480, 24, TFT_BLACK);
  tft.setTextSize(3); tft.setTextColor(0x07FF, TFT_BLACK);
  if (hasNext2() && gpsHasFix) {
    float d = haversineKm(gpsLat, gpsLon, staWin[2].lat, staWin[2].lon) * 1000.0f;
    char buf[24]; snprintf(buf, sizeof(buf), "%.0f m", d);
    tft.drawString(buf, 240, STA_DIST2_Y);
  } else if (hasNext2()) {
    tft.drawString("-- m", 240, STA_DIST2_Y);
  }
}

float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dLat = (lat2 - lat1) * (float)M_PI / 180.0f;
  float dLon = (lon2 - lon1) * (float)M_PI / 180.0f;
  float a = sinf(dLat/2)*sinf(dLat/2)
           + cosf(lat1*(float)M_PI/180.0f)*cosf(lat2*(float)M_PI/180.0f)
           * sinf(dLon/2)*sinf(dLon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

// ── Announcement System ────────────────────────────────────────────────────

// Download audio file from engUrl and save to SD as path. Called with SD SPI active.
static bool downloadAudioToSD(const char* url, const char* path) {
  if (!url || url[0] == '\0') { Serial.println("[AUD-DL] engUrl is empty"); return false; }
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[AUD-DL] WiFi not connected"); return false; }
  Serial.printf("[AUD-DL] %s\n", url);
  HTTPClient http;
  WiFiClientSecure sc; sc.setInsecure();
  WiFiClient       wc;
  bool isHttps = (strncmp(url, "https", 5) == 0);
  if (isHttps) http.begin(sc, url); else http.begin(wc, url);
  http.setTimeout(12000);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[AUD-DL] HTTP %d\n", httpCode);
    http.end(); return false;
  }
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("[AUD-DL] SD open fail");
    http.end(); return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[512];
  int remaining = http.getSize(), total = 0;
  unsigned long t0 = millis();
  while ((remaining < 0 || remaining > 0) && millis() - t0 < 15000UL) {
    int av = stream->available();
    if (av > 0) {
      int n = stream->readBytes(buf, min((int)sizeof(buf), remaining > 0 ? remaining : (int)sizeof(buf)));
      if (n > 0) { f.write(buf, n); total += n; if (remaining > 0) remaining -= n; }
    } else if (!http.connected()) break;
    delay(1);
  }
  f.close(); http.end();
  Serial.printf("[AUD-DL] %d bytes saved to %s\n", total, path);
  return total > 0;
}

// Play WAV from SD card → MAX98357A using the raw ESP-IDF I2S driver.
// No external audio library — avoids ESP8266Audio's ESP32-S3 DMA compatibility issues.
static void playStationAudio(const char* code, const char* engUrl = "") {
  if (code[0] == '\0') return;
  char path[20]; snprintf(path, sizeof(path), "/%s.wav", code);
  Serial.printf("[AUD] play %s\n", path);

  SPI.end();
  SPI.begin(12, SD_MISO, 11, SD_CS);
  if (!SD.begin(SD_CS, SPI, 400000)) {
    Serial.println("[AUD] SD.begin failed");
    SPI.end(); SPI.begin(12, TOUCH_MISO, 11); return;
  }

  File f = SD.open(path);
  if (!f) {
    Serial.printf("[AUD] not found: %s — engUrl='%s'\n", path, engUrl ? engUrl : "(null)");
    if (downloadAudioToSD(engUrl, path)) {
      f = SD.open(path);
    }
    if (!f) {
      SD.end(); digitalWrite(SD_CS, HIGH);
      SPI.end(); SPI.begin(12, TOUCH_MISO, 11); return;
    }
  }

  // ── Parse RIFF/WAVE chunks ─────────────────────────────────────────────
  uint8_t riff[12];
  if (f.read(riff, 12) != 12 || memcmp(riff, "RIFF", 4) || memcmp(riff+8, "WAVE", 4)) {
    Serial.println("[AUD] not a WAV"); goto done;
  }

  {
    uint16_t channels = 1, bitsPerSamp = 16;
    uint32_t sampleRate = 16000, dataSize = 0;
    bool gotFmt = false, gotData = false;

    while (f.available() >= 8 && !gotData) {
      uint8_t  tag[4]; uint32_t csz;
      f.read(tag, 4); f.read((uint8_t*)&csz, 4);
      if (!memcmp(tag, "fmt ", 4) && csz >= 16) {
        uint8_t fb[16]; f.read(fb, 16);
        channels    = fb[2]  | (fb[3]  << 8);
        sampleRate  = fb[4]  | (fb[5]<<8) | (fb[6]<<16) | (fb[7]<<24);
        bitsPerSamp = fb[14] | (fb[15] << 8);
        if (csz > 16) f.seek(f.position() + csz - 16);
        gotFmt = true;
      } else if (!memcmp(tag, "data", 4)) {
        dataSize = csz; gotData = true;
      } else {
        if (csz & 1) csz++;
        f.seek(f.position() + csz);
      }
    }

    if (!gotFmt || !gotData || dataSize == 0) {
      Serial.println("[AUD] WAV parse fail"); goto done;
    }
    Serial.printf("[AUD] %lu Hz %d-bit %s\n",
      (unsigned long)sampleRate, bitsPerSamp, channels==1?"mono":"stereo");

    // ── I2S driver setup ───────────────────────────────────────────────────
    static bool i2sInstalled = false;
    if (i2sInstalled) { i2s_driver_uninstall(I2S_NUM_0); i2sInstalled = false; }

    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = sampleRate;
    cfg.bits_per_sample      = (i2s_bits_per_sample_t)bitsPerSamp;
    cfg.channel_format       = (channels == 1) ? I2S_CHANNEL_FMT_ONLY_LEFT
                                               : I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 8;
    cfg.dma_buf_len          = 1024;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = ANN_I2S_BCK_PIN;    // GPIO 35 → MAX98357A BCLK
    pins.ws_io_num    = ANN_I2S_WS_PIN;     // GPIO 36 → MAX98357A LRC
    pins.data_out_num = ANN_I2S_DOUT_PIN;   // GPIO 37 → MAX98357A DIN
    pins.data_in_num  = I2S_PIN_NO_CHANGE;

    digitalWrite(AMP_RELAY_PIN, HIGH);  // power on amplifier via relay
    delay(150);                          // wait for amp to stabilise

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK ||
        i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
      Serial.println("[AUD] I2S init failed"); goto done;
    }
    i2sInstalled = true;
    i2s_zero_dma_buffer(I2S_NUM_0);

    // ── Stream PCM data ────────────────────────────────────────────────────
    // i2s_write() with 500 ms timeout: blocks briefly then yields to FreeRTOS
    // idle task — WDT resets naturally without any explicit esp_task_wdt_reset().
    static uint8_t ibuf[4096];
    uint32_t rem = dataSize;
    audioStop = false;
    while (rem > 0 && f.available() && !audioStop) {
      uint32_t chunk = (rem < sizeof(ibuf)) ? rem : (uint32_t)sizeof(ibuf);
      size_t   got   = f.read(ibuf, chunk);
      if (got == 0) break;
      if (bitsPerSamp == 16 && volumeLevel < 100) {
        int16_t* s = (int16_t*)ibuf;
        int      n = got / 2;
        float    scale = volumeLevel / 100.0f;
        for (int i = 0; i < n; i++) s[i] = (int16_t)(s[i] * scale);
      }
      size_t written = 0;
      i2s_write(I2S_NUM_0, ibuf, got, &written, pdMS_TO_TICKS(500));
      rem -= got;
    }

    if (!audioStop) delay(150);          // let last DMA buffer finish playing
    i2s_driver_uninstall(I2S_NUM_0);
    i2sInstalled = false;
  }

done:
  digitalWrite(AMP_RELAY_PIN, LOW);   // power off amplifier relay
  f.close();
  SD.end(); digitalWrite(SD_CS, HIGH);
  SPI.end(); SPI.begin(12, TOUCH_MISO, 11);
  Serial.println("[AUD] done");
}

// Highlight the next station name in orange while announcing (no banner popup).
static void annShowBanner(int num) {
  (void)num;
  if (currentScreen != SCREEN_STATION) return;
  tft.setTextDatum(TC_DATUM);
  tft.fillRect(0, STA_NAME1_Y, 480, 42, TFT_BLACK);
  tft.setTextSize(5); tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(staWin[1].name, 240, STA_NAME1_Y);
  annBannerMs = millis();   // start timer so clearAnnBanner() can restore white
}

// Fire one announcement: update state, log, show banner, play audio.
static void fireAnnouncement(int num) {
  annFired  = num;
  annLastMs = millis();
  Serial.printf("[ANN] %d/%d  dist=%.0fm  → %s  code=%s\n",
                num, announceReps,
                haversineKm(gpsLat, gpsLon, staWin[1].lat, staWin[1].lon) * 1000.0f,
                staWin[1].name, staWin[1].code);
  annShowBanner(num);
  playStationAudio(staWin[1].code, staWin[1].engUrl);
}

// Called after each GPS location fix.
// START: fires as soon as train is anywhere in the Far–Near zone (or if trip
//        started already inside the zone).
// RUN:   once started, always completes all Repeats (every Gap seconds) —
//        distance is ignored mid-sequence so narrow zones or GPS noise never
//        interrupt a running sequence.
// RESET: only resets after 3 consecutive readings outside the Far zone
//        (prevents GPS jitter from restarting mid-sequence).
void checkAnnouncement(float distM) {
  if (!hasNext1()) return;
  if (announceDistM <= 0) return;
  if (currentScreen != SCREEN_STATION) { annActive = false; annFired = 0; annOutsideCount = 0; return; }

  // Sequence already running — complete it regardless of current distance
  if (annActive) {
    if (annFired >= announceReps) {
      annActive = false;   // all repeats done — clear orange highlight
      return;
    }
    if (millis() - annLastMs >= (unsigned long)(announceGapSec * 1000UL)) {
      fireAnnouncement(annFired + 1);
    }
    return;
  }

  // Sequence not running — check if train is in the zone to start it
  if (distM > announceDistM) {
    // Outside Far: count consecutive readings before resetting
    annOutsideCount++;
    if (annOutsideCount >= 3) { annFired = 0; annOutsideCount = 0; }
    return;
  }
  annOutsideCount = 0;

  // Inside Near and sequence hasn't started — train already too close, skip
  if (announceNearDistM > 0 && distM <= announceNearDistM) return;

  // Already completed all repeats for this station — don't restart
  if (annFired >= announceReps) return;

  // In zone (Far >= distM > Near) — start sequence immediately
  annActive = true;
  annFired  = 0;
  fireAnnouncement(1);
}

// Called every loop() iteration — auto-clears the banner after ANN_BANNER_MS.
void clearAnnBanner() {
  if (!annBannerMs) return;
  if (millis() - annBannerMs >= ANN_BANNER_MS) {
    annBannerMs = 0;
    if (currentScreen == SCREEN_STATION) drawStationContent();
  }
}

// Speed shown in top-right of the trip header row
void updateSpeedDisplay() {
  tft.fillRect(200, STA_HDR_Y - 12, 268, 26, TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  tft.setTextSize(3); tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  char buf[20];
  if (gpsHasFix) snprintf(buf, sizeof(buf), "%d km/h", (int)roundf(gpsSpeedKmh));
  else           snprintf(buf, sizeof(buf), "-- km/h");
  tft.drawString(buf, 468, STA_HDR_Y);
}

void drawStationScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();

  // Trip number — top-left of content area
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(3); tft.setTextColor(0x07FF, TFT_BLACK);
  tft.drawString(tripName, 16, STA_HDR_Y);

  // Speed — top-right of content area
  updateSpeedDisplay();

  // Back button — bottom-left
  int bcy = STA_BACK_Y + STA_BACK_H / 2;
  tft.fillRoundRect(STA_BACK_X, STA_BACK_Y, STA_BACK_W, STA_BACK_H, 6, 0x2104);
  tft.drawRoundRect(STA_BACK_X, STA_BACK_Y, STA_BACK_W, STA_BACK_H, 6, TFT_WHITE);
  tft.fillTriangle(STA_BACK_X + 10, bcy, STA_BACK_X + 20, bcy - 7, STA_BACK_X + 20, bcy + 7, TFT_WHITE);
  tft.setTextDatum(ML_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, 0x2104);
  tft.drawString("BACK", STA_BACK_X + 24, bcy);

  // Play button — bottom-right
  int pcy = STA_PLAY_Y + STA_PLAY_H / 2;
  tft.fillRoundRect(STA_PLAY_X, STA_PLAY_Y, STA_PLAY_W, STA_PLAY_H, 6, 0x07E0);
  tft.drawRoundRect(STA_PLAY_X, STA_PLAY_Y, STA_PLAY_W, STA_PLAY_H, 6, TFT_WHITE);
  tft.setTextDatum(ML_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_BLACK, 0x07E0);
  tft.drawString("PLAY", STA_PLAY_X + 8, pcy);
  tft.fillTriangle(STA_PLAY_X + STA_PLAY_W - 18, pcy - 7,
                   STA_PLAY_X + STA_PLAY_W - 18, pcy + 7,
                   STA_PLAY_X + STA_PLAY_W - 8,  pcy, TFT_BLACK);

  drawStationContent();
}

// ── Trip Screen Touch ─────────────────────────────────────────────────────────
void handleTripTouch(int tx, int ty) {
  if (tx >= TRIP_BTN_X && tx < TRIP_BTN_X + TRIP_BTN_W &&
      ty >= SEARCH_Y   && ty < SEARCH_Y + SEARCH_H) {
    if (inputLen > 0) {
      tripFound = searchTrip(inputText);
      currentScreen = SCREEN_TRIP_DETAIL;
      drawTripDetailScreen();
    }
    return;
  }
  if (kbRowTouch(tx, ty, inputText, inputLen, 63)) { updateSearchText(); return; }
  switch (kbBottomTouch(tx, ty)) {
    case 1: currentScreen = SCREEN_MENU; inputLen = 0; inputText[0] = '\0'; drawHeader(); drawMenu(); break;
    case 2: if (inputLen < 63) { inputText[inputLen++] = ' '; inputText[inputLen] = '\0'; } updateSearchText(); break;
    case 3: {
      if      (kbMode == 3) kbMode = 4;  // nums → UPPER alpha
      else if (kbMode == 4) kbMode = 5;  // UPPER → lower alpha
      else if (kbMode == 5) kbMode = 2;  // lower → symbols
      else                  kbMode = 3;  // symbols → back to numbers
      drawKeyboard(); break;
    }
    case 4: inputLen = 0; inputText[0] = '\0'; updateSearchText(); break;
  }
}

// ── Trip Detail Touch ─────────────────────────────────────────────────────────
void handleTripDetailTouch(int tx, int ty) {
  if (backButtonHit(tx, ty)) {
    currentScreen = SCREEN_TRIP;
    drawTripScreen();
    return;
  }
  if (tripFound) {
    if (tx >= TD_START_X && tx < TD_START_X + TD_START_W &&
        ty >= TD_BTN_Y   && ty < TD_BTN_Y + TD_BTN_H) {
      currentScreen = SCREEN_STATION;
      drawStationScreen();
      return;
    }
    if (tx >= TD_UPDATE_X && tx < TD_UPDATE_X + TD_UPDATE_W &&
        ty >= TD_BTN_Y    && ty < TD_BTN_Y + TD_BTN_H) {
      downloadAndSaveTrip(inputText);
      return;
    }
  } else {
    if (tx >= DL_BTN_X && tx < DL_BTN_X + DL_BTN_W &&
        ty >= DL_BTN_Y && ty < DL_BTN_Y + DL_BTN_H) {
      downloadAndSaveTrip(inputText);
      return;
    }
  }
}

// ── Touch Helper ─────────────────────────────────────────────────────────────
bool stableTouch(int &tx, int &ty) {
  if (!ts.touched()) return false;
  delay(30);
  if (!ts.touched()) return false;
  long sumX = 0, sumY = 0;
  int  n    = 0;
  for (int i = 0; i < 8; i++) {
    if (ts.touched()) {
      TS_Point p = ts.getPoint();
      sumX += p.x;
      sumY += p.y;
      n++;
    }
    delay(5);
  }
  if (n == 0) return false;
  tx = constrain(map(sumX / n, 3800, 300, 480, 0), 0, 479);
  ty = constrain(map(sumY / n, 3800, 300, 320, 0), 0, 319);
  while (ts.touched()) delay(10);
  delay(40);
  return true;
}

// ── Splash Screen ─────────────────────────────────────────────────────────────
#define SPLASH_BAR_X  60
#define SPLASH_BAR_Y 230
#define SPLASH_BAR_W 360
#define SPLASH_BAR_H  18

void drawSplashBase() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Train Info", 240, 100);
  tft.setTextSize(2);
  tft.setTextColor(0x7BEF, TFT_BLACK);
  tft.drawString("GPS Display System", 240, 148);
  tft.drawRoundRect(SPLASH_BAR_X, SPLASH_BAR_Y, SPLASH_BAR_W, SPLASH_BAR_H, 6, TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("MindCoin", 240, 310);
}

void splashProgress(int pct, const char* status) {
  int fillW = pct * (SPLASH_BAR_W - 4) / 100;
  if (fillW > 0)
    tft.fillRoundRect(SPLASH_BAR_X + 2, SPLASH_BAR_Y + 2, fillW, SPLASH_BAR_H - 4, 4, TFT_CYAN);
  tft.fillRect(0, 262, 480, 24, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(status, 240, 274);
  tft.fillRect(200, 295, 80, 20, TFT_BLACK);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(buf, 240, 305);
}

// ── GPS Info Screen ───────────────────────────────────────────────────────────
void drawGpsScreen() {
  tft.fillRect(0, HEADER_H, 480, 320 - HEADER_H, TFT_BLACK);
  drawHeader();
  tft.setTextDatum(MC_DATUM);

  // Title
  tft.setTextSize(3);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("GPS Info", 240, HEADER_H + 26);
  tft.drawFastHLine(20, HEADER_H + 48, 440, 0x39C7);

  // Fix status badge
  const char* fixLabel = gpsHasFix ? "GPS OK" : "No Fix";
  uint16_t    fixBg    = gpsHasFix ? 0x0460  : 0xA000;
  tft.fillRoundRect(140, HEADER_H + 56, 200, 40, 10, fixBg);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, fixBg);
  tft.drawString(fixLabel, 240, HEADER_H + 76);

  // Satellites label + count
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Satellites", 240, HEADER_H + 118);

  uint16_t satColor = (gpsSatCount >= 4) ? TFT_GREEN
                    : (gpsSatCount >  0) ? TFT_ORANGE
                    :                      TFT_RED;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", gpsSatCount);
  tft.setTextSize(5);
  tft.setTextColor(satColor, TFT_BLACK);
  tft.drawString(buf, 240, HEADER_H + 162);

  // Signal level
  tft.drawFastHLine(20, HEADER_H + 194, 440, 0x39C7);
  const char* sigLabel[] = { "No Signal", "Weak Signal", "Good Signal" };
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(sigLabel[signalLevel], 240, HEADER_H + 212);

  drawBackButton();
}

// Lightweight refresh — only repaints the values that change each second
static void updateGpsScreen() {
  tft.setTextDatum(MC_DATUM);

  // Fix status badge
  const char* fixLabel = gpsHasFix ? "GPS OK" : "No Fix";
  uint16_t    fixBg    = gpsHasFix ? 0x0460  : 0xA000;
  tft.fillRoundRect(140, HEADER_H + 56, 200, 40, 10, fixBg);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, fixBg);
  tft.drawString(fixLabel, 240, HEADER_H + 76);

  // Satellite count
  tft.fillRect(160, HEADER_H + 138, 160, 50, TFT_BLACK);
  uint16_t satColor = (gpsSatCount >= 4) ? TFT_GREEN
                    : (gpsSatCount >  0) ? TFT_ORANGE
                    :                      TFT_RED;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", gpsSatCount);
  tft.setTextSize(5);
  tft.setTextColor(satColor, TFT_BLACK);
  tft.drawString(buf, 240, HEADER_H + 162);

  // Signal level label
  tft.fillRect(80, HEADER_H + 200, 320, 24, TFT_BLACK);
  const char* sigLabel[] = { "No Signal", "Weak Signal", "Good Signal" };
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(sigLabel[signalLevel], 240, HEADER_H + 212);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(100);

  // ── Hold BOTH CS pins HIGH before ANY SPI activity ────────────────────────
  // SD_CS must be HIGH before TFT/touch SPI starts — floating CS corrupts card
  pinMode(SD_CS,       OUTPUT); digitalWrite(SD_CS,       HIGH);
  pinMode(TOUCH_CS,    OUTPUT); digitalWrite(TOUCH_CS,    HIGH);
  pinMode(AMP_RELAY_PIN, OUTPUT); digitalWrite(AMP_RELAY_PIN, LOW);  // amp off at boot
  delay(50);

  // ── Init SD FIRST on a clean bus — before TFT or touch ───────────────────
  // SD spec: card must receive ≥74 clock pulses at ≤400 kHz after power-on.
  // Re-init the full SPI bus before each attempt so a previous partial-init
  // never leaves the card in an unknown state.
  Serial.println("[SD] Connecting...");
  delay(250);   // power-on stabilisation — cards need ≥250 ms after Vcc rises

  sdReady = false;
  for (int attempt = 1; attempt <= 5; attempt++) {
    SPI.end();
    delay(10);
    SPI.begin(12, SD_MISO, 11, SD_CS);
    delay(20);
    if (SD.begin(SD_CS, SPI, 400000)) {   // 400 kHz — mandatory SD init speed
      sdReady = true;
      Serial.printf("[SD] OK (attempt %d)  size=%llu MB  type=%d\n",
                    attempt,
                    SD.cardSize() / (1024ULL * 1024ULL), (int)SD.cardType());
      SD.end();
      break;
    }
    Serial.printf("[SD] Attempt %d failed — retrying...\n", attempt);
    SD.end();
    digitalWrite(SD_CS, HIGH);
    delay(500);
  }
  if (!sdReady) {
    Serial.println("[SD] ERROR: SD init failed after 5 attempts");
    Serial.println("[SD]   CS=GPIO15  MISO=GPIO40  SCK=GPIO12  MOSI=GPIO11");
  }
  digitalWrite(SD_CS, HIGH);
  SPI.end();
  delay(20);

  // ── Backlight ─────────────────────────────────────────────────────────────
  ledcAttach(TFT_BL_PIN, 5000, 8);
  ledcWrite(TFT_BL_PIN, 255);

  // ── TFT + Touch ───────────────────────────────────────────────────────────
  SPI.begin(12, TOUCH_MISO, 11);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Mind Coin", 240, 150);
  tft.setTextSize(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Powered by MindCoin", 240, 200);
  delay(2000);

  tft.fillScreen(TFT_BLACK);
  drawSplashBase();
  splashProgress(5, "Starting...");

  ts.begin(SPI);
  SPI.begin(12, TOUCH_MISO, 11);
  splashProgress(30, "Touch OK");
  delay(300);

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX, false, 512);
  splashProgress(45, "GPS Ready");

  prefs.begin("trainapp", false);

  splashProgress(55, "WiFi...");
  loadWifiNetworks();
  if (wifiCount > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSIDs[0], wifiPasses[0]);
    splashProgress(70, "WiFi Connecting...");
  } else {
    splashProgress(70, "No WiFi Configured");
  }
  delay(300);

  // ── SD status on splash ───────────────────────────────────────────────────
  if (sdReady) {
    splashProgress(75, "SD Card OK");
  } else {
    splashProgress(75, "SD Card FAIL");
  }

  splashProgress(80, "Battery ADC...");
  analogSetAttenuation(ADC_11db);
  batteryPercent = readBatteryPercent();

  volumeLevel    = prefs.getInt("volume",    50);
  announceDistM     = prefs.getFloat("annDist",     500.0f);
  announceNearDistM = prefs.getFloat("annNearDist", 150.0f);
  announceReps      = prefs.getInt("annReps",      4);
  announceGapSec    = prefs.getInt("annGapSec",    10);
  approachDistM  = prefs.getFloat("approachDist",  500.0f);
  gpsMatchDistM  = prefs.getFloat("gpsMatchDist",  200.0f);

  splashProgress(100, "Ready!");
  delay(700);

  drawHeader();
  drawMenu();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // ── GPS: drain entire serial buffer first — no SD access inside this loop ──
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    lastGpsDataMs = millis();
  }

  // ── Auto-clear announcement banner after ANN_BANNER_MS ───────────────────
  clearAnnBanner();

  // ── Deferred station advance — runs after GPS buffer is fully drained ──────
  if (pendingAdvance) {
    pendingAdvance = false;
    loadOneStation(staWin[1].seqNo + 1, staWin[2]);
    Serial.printf("[STA] Advanced → next=%s upcoming=%s\n",
                  hasNext1() ? staWin[1].name : "end",
                  hasNext2() ? staWin[2].name : "end");
    if (currentScreen == SCREEN_STATION) drawStationContent();
  }

  // ── Process GPS sentences decoded above ────────────────────────────────────
  if (gps.satellites.isUpdated()) {
    int sats  = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
    gpsSatCount = sats;
    int level = (sats == 0) ? 0 : (sats < 4) ? 1 : 2;
    if (level != signalLevel) updateSignal(level);
  }
  if (gps.location.isUpdated()) {
    bool newFix = gps.location.isValid();
    if (newFix) {
      lastGpsFixMs = millis();
      gpsLat = (float)gps.location.lat();
      gpsLon = (float)gps.location.lng();
      if (hasNext1()) {
        float distM = haversineKm(gpsLat, gpsLon,
                        staWin[1].lat, staWin[1].lon) * 1000.0f;
        if (distM < approachDistM) advanceStation();  // sets pendingAdvance only
        checkAnnouncement(distM);
      }
    }
    if (newFix != gpsHasFix) {
      gpsHasFix = newFix;
      if (currentScreen == SCREEN_STATION) drawStationContent();
    } else if (newFix && currentScreen == SCREEN_STATION) {
      updateDistancesDisplay();
    }
  }
  if (gps.speed.isUpdated()) {
    float newSpeed = (gpsHasFix && gps.speed.isValid()) ? gps.speed.kmph() : 0.0f;
    if (fabsf(newSpeed - gpsSpeedKmh) >= 0.5f) {
      gpsSpeedKmh = newSpeed;
      if (currentScreen == SCREEN_STATION) updateSpeedDisplay();
    }
  }

  // GPS watchdog — 5 s silence → No GPS (module stopped sending)
  if (lastGpsDataMs > 0 && millis() - lastGpsDataMs > 5000UL) {
    lastGpsDataMs = 0;
    if (gpsHasFix) {
      gpsHasFix   = false;
      gpsSatCount = 0;
      if (currentScreen == SCREEN_STATION) {
        updateDistancesDisplay();
        updateSpeedDisplay();
      }
    }
    if (signalLevel != 0) updateSignal(0);
  }
  // Fix watchdog — module still sending but no valid location for 5 min → show --m
  if (gpsHasFix && lastGpsFixMs > 0 && millis() - lastGpsFixMs > 300000UL) {
    gpsHasFix = false;
    if (currentScreen == SCREEN_STATION) {
      updateDistancesDisplay();
      updateSpeedDisplay();
    }
  }

  if (millis() - lastWifiCheck >= 5000UL) {
    lastWifiCheck = millis();
    bool nowConn = (WiFi.status() == WL_CONNECTED);
    int  nowSig  = 0;
    if (nowConn) {
      int rssi = WiFi.RSSI();
      nowSig = (rssi > -50) ? 4 : (rssi > -65) ? 3 : (rssi > -75) ? 2 : (rssi > -85) ? 1 : 0;
    }
    if (nowConn != wifiConnected || nowSig != wifiSigLevel) {
      wifiConnected = nowConn;
      wifiSigLevel  = nowSig;
      drawInternetSignal();
      if (currentScreen == SCREEN_WIFI) drawWifiScreen();
    }
  }

  static unsigned long lastBatCheck = 0;
  if (millis() - lastBatCheck >= 5000UL) {
    lastBatCheck = millis();
    int pct = readBatteryPercent();
    if (pct != batteryPercent) updateBattery(pct);
  }

  // Refresh GPS info screen every second
  static unsigned long lastGpsRefresh = 0;
  if (currentScreen == SCREEN_GPS && millis() - lastGpsRefresh >= 1000UL) {
    lastGpsRefresh = millis();
    updateGpsScreen();
  }

  int tx, ty;
  if (!stableTouch(tx, ty)) return;

  if (currentScreen == SCREEN_MENU) {
    int idx = menuItemAt(tx, ty);
    if      (idx == 0) { currentScreen = SCREEN_TRIP;     drawTripScreen();     }
    else if (idx == 1) { currentScreen = SCREEN_SETTINGS; drawSettingsScreen(); }
  } else if (currentScreen == SCREEN_SETTINGS) {
    if (backButtonHit(tx, ty)) {
      currentScreen = SCREEN_MENU;
      drawMenu();
    } else {
      int si;
      if (settingButtonHit(tx, ty, si)) {
        if      (si == 0) { currentScreen = SCREEN_VOLUME;   drawVolumeScreen();   }
        else if (si == 1) { currentScreen = SCREEN_ANNOUNCE; drawAnnounceScreen(); }
        else if (si == 2) { currentScreen = SCREEN_WIFI;     drawWifiScreen();     }
        else if (si == 3) { currentScreen = SCREEN_GPS;      drawGpsScreen();      }
        else if (si == 4) { currentScreen = SCREEN_APPROACH;  drawApproachScreen(); }
        else if (si == 5) { currentScreen = SCREEN_GPS_MATCH; drawGmsScreen();      }
      }
    }
  } else if (currentScreen == SCREEN_VOLUME) {
    if (backButtonHit(tx, ty)) {
      currentScreen = SCREEN_SETTINGS;
      drawSettingsScreen();
    } else if (tx >= VOL_MINUS_X && tx < VOL_MINUS_X + VOL_MINUS_W &&
               ty >= VOL_MINUS_Y && ty < VOL_MINUS_Y + VOL_MINUS_H) {
      applyVolume(volumeLevel - 5);
    } else if (tx >= VOL_PLUS_X && tx < VOL_PLUS_X + VOL_PLUS_W &&
               ty >= VOL_PLUS_Y && ty < VOL_PLUS_Y + VOL_PLUS_H) {
      applyVolume(volumeLevel + 5);
    } else if (ty >= VOL_BAR_Y - 20 && ty <= VOL_BAR_Y + VOL_BAR_H + 20 &&
               tx >= VOL_BAR_X      && tx <= VOL_BAR_X + VOL_BAR_W) {
      int newVol = (tx - VOL_BAR_X) * 100 / VOL_BAR_W;
      applyVolume(newVol);
    }
  } else if (currentScreen == SCREEN_TRIP) {
    handleTripTouch(tx, ty);
  } else if (currentScreen == SCREEN_TRIP_DETAIL) {
    handleTripDetailTouch(tx, ty);
  } else if (currentScreen == SCREEN_STATION) {
    if (tx >= STA_BACK_X && tx < STA_BACK_X + STA_BACK_W &&
        ty >= STA_BACK_Y && ty < STA_BACK_Y + STA_BACK_H) {
      audioStop = true;
      annActive = false; annFired = 0; annBannerMs = 0; annOutsideCount = 0;
      inputText[0] = '\0';
      currentScreen = SCREEN_TRIP;
      drawTripScreen();
    } else if (tx >= STA_PLAY_X && tx < STA_PLAY_X + STA_PLAY_W &&
               ty >= STA_PLAY_Y && ty < STA_PLAY_Y + STA_PLAY_H && hasNext1()) {
      Serial.printf("[PLAY-TEST] code=%s\n", staWin[1].code);
      playStationAudio(staWin[1].code, staWin[1].engUrl);
    }
  } else if (currentScreen == SCREEN_NOT_FOUND) {
    if (backButtonHit(tx, ty)) {
      currentScreen = SCREEN_TRIP;
      drawTripScreen();
    } else if (tx >= DL_BTN_X && tx < DL_BTN_X + DL_BTN_W &&
               ty >= DL_BTN_Y && ty < DL_BTN_Y + DL_BTN_H) {
      downloadAndSaveTrip(inputText);
    }
  } else if (currentScreen == SCREEN_GPS) {
    if (backButtonHit(tx, ty)) {
      currentScreen = SCREEN_SETTINGS;
      drawSettingsScreen();
    }
  } else if (currentScreen == SCREEN_APPROACH) {
    handleApproachTouch(tx, ty);
  } else if (currentScreen == SCREEN_GPS_MATCH) {
    handleGmsTouch(tx, ty);
  } else if (currentScreen == SCREEN_ANNOUNCE) {
    handleAnnounceTouch(tx, ty);
  } else if (currentScreen == SCREEN_WIFI) {
    handleWifiTouch(tx, ty);
  } else if (currentScreen == SCREEN_WIFI_SSID) {
    handleWifiSSIDTouch(tx, ty);
  } else if (currentScreen == SCREEN_WIFI_PASS) {
    handleWifiPassTouch(tx, ty);
  }
}
