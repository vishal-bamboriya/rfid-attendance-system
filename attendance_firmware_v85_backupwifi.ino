/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM - Beta 3.0 (v4.2)
 * PROXY ARCHITECTURE + RAM OPTIMIZATION PASS
 *
 * v4   -> introduced the Cloudflare Worker proxy to cut the
 *         double-TLS-handshake delay (10-30s -> ~6s).
 * v4.1 -> shrank WiFiClientSecure's TLS buffers (32KB -> 1.5KB)
 *         to reduce heap fragmentation.
 * v4.2 -> further RAM/fragmentation cleanup:
 *   - Replaced Arduino String with fixed-size char[] buffers
 *     everywhere in the hot path (UID, URL, JSON field values).
 *   - Wrapped all Serial.print literal strings in F() so they
 *     live in flash instead of being copied into RAM at boot.
 *   - Shrank the ArduinoJson document from 512 to 256 bytes.
 *   - Added ESP.getMaxFreeBlockSize()/getHeapFragmentation()
 *     logging so fragmentation can be measured directly.
 *   - (Over-)shrank WiFiClientSecure buffers to 1024/512, which
 *     turned out to cause handshake stalls on higher-latency
 *     networks (see v4.3).
 * v4.3 -> fixed the v4.2 buffer regression (partially):
 *   - Root-caused via a same-network A/B test: a browser on the
 *     SAME hotspot completed the same request fast every time,
 *     while the ESP8266 hung for 10-23s. That ruled out the
 *     backend, DNS, and signal strength (RSSI was -40 to -53 dBm,
 *     excellent) -- leaving only the device's own TLS handshake.
 *   - Grew the TLS RX buffer since 1024 bytes couldn't fit
 *     Cloudflare's certificate chain in one record.
 * v5.0 (this file) -> final confirmed fixes, found via further
 *   testing:
 *   - THE REAL ROOT CAUSE of the "-11 Read Timeout" failures:
 *     WiFi.setSleepMode(WIFI_NONE_SLEEP). ESP8266 puts its WiFi
 *     radio into modem-sleep automatically when idle to save power.
 *     After 5-10s between card taps, the radio couldn't wake up
 *     fast enough for the next request. This was the dominant cause
 *     all along -- buffer size only ever changed how OFTEN it hit.
 *   - TLS buffer size settled at 5120/512 bytes: the sweet spot
 *     that reliably fits Cloudflare's full certificate (1024 was
 *     too small and truncated it; 16KB default fragmented the heap
 *     and caused reboots).
 *   - WiFiClient::stopAll() before each new connection, to force-
 *     close any stale socket Cloudflare had already dropped on its
 *     end (fixed remaining GET failed: -1 Connection Failed cases).
 *   - https.setReuse(false) + "Connection: close" header + a
 *     normal browser User-Agent: Cloudflare was treating the
 *     ESP8266's bare default headers as bot-like traffic and
 *     slowing/blocking it.
 *   - OLED rendering reworked to auto-fit text size to the 128px
 *     screen width (fixes "Please wait" getting clipped) and to
 *     show the person's name on its own large centered line
 *     instead of being squeezed into small text.
 * v6.0 (this file) -> Mega + LCD + buttons integration:
 *   - Added a simple '@'-prefixed line protocol over the existing
 *     UART (RX/TX, shared with USB debug -- this board only has one
 *     hardware UART) to talk to an Arduino Mega running its own LCD.
 *   - NodeMCU -> Mega: STATUS (every scan result), sent right after
 *     handleResult() processes it.
 *   - Mega -> NodeMCU: BTN,1 (show previous log -> replies LOG,...)
 *     and BTN,2 (system status -> replies SYSINFO,...). Handled via
 *     a non-blocking line reader (checkMegaMessages()) called every
 *     loop() iteration.
 *   - lastResult is cached in RAM so Button 1 doesn't need a fresh
 *     Apps Script call -- it just replays the last known result.
 * v6.1 -> reliability improvement for the Uno link:
 *   - Switched from purely event-driven STATUS messages to a
 *     "heartbeat" model: the current display state (READY,
 *     PROCESSING, or the last scan result) is cached in RAM and
 *     re-broadcast every ~1.5s automatically, in addition to being
 *     sent immediately on every real event. If the Uno misses one
 *     message (e.g. it briefly reset from a power glitch), it
 *     self-corrects on the next heartbeat instead of staying stuck
 *     on stale/wrong text until the next real scan happens.
 * v7.0 -> software watchdog:
 *   - Insurance against unknown/rare hangs (e.g. a WiFi/TLS library
 *     edge case that never returns) -- not a fix for any specific
 *     known bug, just a safety net. Uses a Ticker interrupt to check
 *     every 1s whether loop() has run recently; force-restarts the
 *     board via ESP.restart() if it's been stuck for 40s+ (well
 *     above the ~24s worst case for one scan-to-result cycle
 *     including the automatic retry).
 * v8.0 -> OLED/UX overhaul, now that the secondary LCD
 *   display handles detailed text (name, greeting, scrolling for
 *   long names):
 *   - OLED switched from text screens to icon-based graphics: an
 *     idle "tap card" icon, a static processing (hourglass) icon,
 *     and animated checkmark/cross "flourish" graphics for
 *     success/error results. The two displays now complement each
 *     other (LCD = detail, OLED = quick glance) instead of showing
 *     the same text twice.
 *   - Buzzer tones reworked: a distinct ascending 3-note chime for
 *     success, a clearly different double-buzz for errors, and a
 *     quick high chirp to acknowledge a tap -- easier to tell apart
 *     by ear without looking at either screen.
 * v8.1 -> replaced the boot checklist (tried briefly,
 *   then removed on request) with an animated WiFi "signal bars" icon
 *   that plays for real, synced to connectWiFi()'s actual connection
 *   attempts -- more useful and better-looking than a static text
 *   list, and tied to genuine state instead of a one-time check. The
 *   PING/PONG secondary-display handshake that only existed to
 *   support the checklist has been removed entirely along with it --
 *   there was never any other place in the system that waits on the
 *   secondary display, so removing it changes nothing about runtime
 *   behavior; the secondary display being off or disconnected still
 *   never affects normal attendance operation.
 * v8.2 -> icon redesign: the processing/checkmark/cross
 *   icons looked thin and rough as single-pixel line drawings at
 *   this resolution. Replaced with a consistent "filled badge" style
 *   (a bold white disc with the glyph cut out in black) -- closer to
 *   a Material Design filled status icon (check_circle / cancel)
 *   than a hand-drawn outline. A shared oledBadgeGrowIn() gives all
 *   three a smooth "pop in" flourish, and oledThickLine() fakes
 *   stroke width (Adafruit_GFX has no native line thickness) so
 *   strokes read as bold and smooth instead of hairline-thin.
 * v8.3 -> theme consistency pass:
 *   - Processing ring was too thick/heavy; slimmed from 9px to 5px
 *     for a more elegant Material-style loading ring.
 *   - The WiFi-connecting icon previously used a different visual
 *     language (plain outline arcs, no filled backdrop) from the
 *     other icons. Rebuilt it into the same filled-badge family: a
 *     white disc with a black "signal dot + arcs" glyph cut into it,
 *     matching processing/success/error. The final "connected" frame
 *     also gets the same badge grow-in flourish as success/error, so
 *     it feels like part of the same icon set instead of a one-off.
 * v8.4 (this file):
 *   - Button 1 repurposed from "previous log" to "system info + QR".
 *     Pressing it now shows a QR code on this board's own OLED,
 *     encoding live diagnostics as plain text (WiFi status, RSSI,
 *     uptime, free heap) -- no hosted page needed since there isn't
 *     one yet; swap to a URL later if a docs/GitHub page exists. The
 *     LCD simultaneously gets the same info as readable text via the
 *     existing SYSINFO path. The old lastResult/"previous log" cache
 *     and BTN,2 handling are removed as dead code.
 *   - New: proactive "WiFi connected but no internet" detection.
 *     Previously the only way to notice this was tapping a card and
 *     waiting ~10-20s for a connection error. Now a lightweight plain-
 *     HTTP probe (Google's generate_204 endpoint) runs every 30s while
 *     idle (bounded to a 3s timeout, never during an actual scan) and
 *     shows a small warning badge on the OLED idle screen plus
 *     "No Internet" text on the LCD if it fails.
 *   - Fixed a real bug found while wiring the above in: after a scan,
 *     the OLED went back to its idle icon, but nothing told the LCD
 *     to do the same -- so the LCD (via heartbeat) kept re-showing
 *     the last scan's result indefinitely instead of returning to
 *     "Ready". New goIdle() helper keeps both displays in sync.
 * v8.5 (this file) -> secondary/backup WiFi network support: added
 *   WIFI_SSID2/WIFI_PASSWORD2 (blank by default, safe to leave empty
 *   -- the secondary is simply skipped if so). connectWiFi() now
 *   alternates between the primary and secondary network every time
 *   an attempt times out (15s), so if the primary network is
 *   temporarily down, a configured backup network can take over
 *   instead of the kiosk being stuck retrying a dead network forever.
 * ==========================================
 * Card tap -> WiFi check -> call Worker proxy ->
 * parse JSON -> OLED/buzzer feedback.
 */

#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Ticker.h> // for the software watchdog timer
#include <qrcode.h> // "QRCode" library by Richard Moore -- install via
                    // Arduino Library Manager (search "QRCode")

// ---------- RFID ----------
#define SS_PIN   D4
#define RST_PIN  D3
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------- Buzzer ----------
#define BUZZER_PIN D0

// ---------- OLED (0.96", 128x64, SPI) ----------
// Shares the same hardware SPI bus (SCK=D5, MOSI=D7) as the RFID
// reader -- only needs its own CS/DC/RES pins.
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_CS  D8
#define OLED_DC  D2
#define OLED_RES D1
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RES, OLED_CS);

// ---------- WiFi ----------
const char* WIFI_SSID     = "Xiaomi blossom";
const char* WIFI_PASSWORD = "";

// Secondary/backup network -- fill these in if you have a second WiFi
// available (e.g. a different hotspot or router). If WIFI_SSID2 is
// left empty, the secondary network is simply skipped and only the
// primary is tried, so this is safe to leave blank.
const char* WIFI_SSID2     = ""; // fill in secondary network name
const char* WIFI_PASSWORD2 = ""; // fill in secondary network password

// ---------- Cloudflare Worker proxy ----------
// GSCRIPT_ID lives ONLY as an environment variable inside the
// Cloudflare Worker's dashboard (Settings -> Variables -> GSCRIPT_ID),
// not in this firmware.
const char* HOST = "att-beta3-proxy.ostudy597.workers.dev";
const int HTTPS_PORT = 443;
const char* API_KEY = "RFID2026"; // must match Settings sheet "API Key"

// Debounce so one tap doesn't fire multiple requests.
unsigned long lastScanTime = 0;
const unsigned long SCAN_COOLDOWN = 5000; // ms, applies AFTER result is shown

// ================= Fixed-size buffer limits =================
// Sized generously for real values (UID hex strings are ~8-14 chars,
// names/messages from the sheet are short) with headroom, while still
// being tiny compared to String's heap churn.
#define UID_BUF_LEN      24
#define URL_BUF_LEN      160
#define FIELD_BUF_LEN    32
#define MESSAGE_BUF_LEN  64
#define PAYLOAD_BUF_LEN  384

// ================= Result struct =================
// (Must stay near the top -- see Arduino auto-prototype note)
// Plain char arrays instead of String -- no heap allocation per result.

struct AttendanceResult {
  bool requestOk;
  bool success;
  char type[FIELD_BUF_LEN];
  char name[FIELD_BUF_LEN];
  char time[FIELD_BUF_LEN];
  char hours[FIELD_BUF_LEN];
  char message[MESSAGE_BUF_LEN];
};

// ================= Heartbeat state (for reliable Uno sync) =================
// Instead of only sending a STATUS message when something changes
// (event-driven), we keep the CURRENT display state here and
// re-broadcast it repeatedly. This way, if one message gets lost due
// to a brief reset/noise glitch on the Uno side, it self-corrects
// within a second or two instead of staying stuck forever on stale
// text until the next real event happens.
char currentType[FIELD_BUF_LEN]    = "READY";
char currentName[FIELD_BUF_LEN]    = "";
char currentDetail[MESSAGE_BUF_LEN] = "Tap your card";

unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 1500; // ms

// ================= WiFi =================

void connectWiFi() {
  Serial.print(F("Connecting to WiFi"));
  WiFi.mode(WIFI_STA);

  // CONFIRMED ROOT CAUSE of the "-11 Read Timeout" failures: ESP8266
  // puts the WiFi radio into modem-sleep automatically after a period
  // of idle time to save power. When the next card was tapped 5-10s
  // later, the radio couldn't wake up fast enough, so the connection
  // attempt stalled. Keeping the radio always-on fixes this outright.
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Use Google's public DNS instead of whatever the hotspot hands out --
  // mobile hotspots sometimes have slow/flaky DNS resolvers, which would
  // add delay BEFORE the TLS handshake even starts (and wouldn't be
  // bounded by our WiFiClientSecure/HTTPClient timeouts at all).
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(0U, 0U, 0U, dns1, dns2); // 0U = keep DHCP-assigned IP/gateway/subnet, override DNS only

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F(" ["));
  Serial.print(WIFI_SSID);
  Serial.print(F("]"));

  bool haveSecondary = (strlen(WIFI_SSID2) > 0);
  bool usingSecondary = false;

  unsigned long startAttempt = millis();
  int wifiAnimFrame = 0;
  while (WiFi.status() != WL_CONNECTED) {
    oledWifiConnectingFrame(wifiAnimFrame % 4); // animated signal-bars icon
    wifiAnimFrame++;
    delay(300);
    Serial.print(F("."));
    if (millis() - startAttempt > 15000) {
      // Alternate to the other network (if a secondary one is
      // configured) each time the current attempt times out, so a
      // temporarily-down primary network doesn't block forever if a
      // working backup network is available.
      if (haveSecondary) {
        usingSecondary = !usingSecondary;
      }
      const char* trySsid = usingSecondary ? WIFI_SSID2 : WIFI_SSID;
      const char* tryPass = usingSecondary ? WIFI_PASSWORD2 : WIFI_PASSWORD;

      Serial.println();
      Serial.print(F("WiFi timeout, retrying on ["));
      Serial.print(trySsid);
      Serial.println(F("]..."));
      WiFi.disconnect();
      delay(500);
      WiFi.begin(trySsid, tryPass);
      startAttempt = millis();
    }
  }

  // Brief "connected" flourish -- full signal + hold, before handing
  // off to the normal idle screen.
  oledWifiConnectingFrame(3);
  delay(500);

  Serial.println();
  Serial.print(F("WiFi connected. IP: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("Signal strength (RSSI): "));
  Serial.print(WiFi.RSSI());
  Serial.println(F(" dBm"));
}

void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi lost, reconnecting..."));
    connectWiFi();
  }
}

// ================= Software Watchdog =================
// Insurance against unknown/rare hangs (e.g. a WiFi/TLS library edge
// case that never returns) that aren't caused by anything we've
// identified so far, but can't be fully ruled out either -- this is
// a low-cost safety net, not a fix for any known bug.
//
// How it works: loop() updates lastLoopRunTime every iteration. A
// Ticker interrupt checks every second whether too much time has
// passed since the last update. If loop() is well and truly stuck
// (not just slow -- a normal scan can take several seconds, which is
// fine), the watchdog force-restarts the board rather than leaving
// it hung indefinitely with no way to recover except a manual
// power-cycle.
//
// WATCHDOG_TIMEOUT is set well above the worst-case time we've
// actually observed for one full scan-to-result cycle (including
// retries), so it will never fire during normal operation --
// it only trips if something is truly stuck.
Ticker watchdogTicker;
volatile unsigned long lastLoopRunTime = 0;
const unsigned long WATCHDOG_TIMEOUT = 40000; // ms

void checkWatchdog() {
  if (millis() - lastLoopRunTime > WATCHDOG_TIMEOUT) {
    // Can't reliably do Serial output from inside a Ticker ISR context
    // on ESP8266, so we just restart directly.
    ESP.restart();
  }
}



void logHeapStatus(const __FlashStringHelper* label) {
  Serial.print(label);
  Serial.print(F(" | Free heap: "));
  Serial.print(ESP.getFreeHeap());
  Serial.print(F(" | Max free block: "));
  Serial.print(ESP.getMaxFreeBlockSize());
  Serial.print(F(" | Fragmentation: "));
  Serial.print(ESP.getHeapFragmentation());
  Serial.print(F("% | RSSI: "));
  Serial.print(WiFi.RSSI());
  Serial.println(F(" dBm"));
}

// ================= Internet connectivity monitor =================
// WiFi being "connected" (associated to the router) does NOT
// guarantee actual internet access -- e.g. router has no WAN link,
// ISP outage, captive portal, etc. Previously the only way to notice
// this was tapping a card and waiting ~10-20s for a connection error.
// This periodically (while idle) makes a small plain-HTTP request to
// Google's generate_204 endpoint -- a tiny, widely-used connectivity
// probe (returns HTTP 204 with an empty body if the internet is
// actually reachable) -- to catch "WiFi connected but no internet"
// proactively, without waiting for a real scan to fail.
bool internetOk = true;
unsigned long lastConnCheck = 0;
const unsigned long CONN_CHECK_INTERVAL = 30000; // ms, only while idle

bool checkInternetNow() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client; // plain HTTP, no TLS -- faster and avoids
                     // unnecessary handshake overhead for a pure
                     // connectivity probe
  HTTPClient http;
  http.setTimeout(3000); // bounded -- never blocks more than ~3s

  bool ok = false;
  if (http.begin(client, "http://connectivitycheck.gstatic.com/generate_204")) {
    int code = http.GET();
    ok = (code == 204 || code == 200);
    http.end();
  }
  return ok;
}

// Call periodically from loop() -- only actually probes every
// CONN_CHECK_INTERVAL ms, and only while idle (not mid-scan), so it
// never adds delay to an actual attendance tap.
void maybeCheckInternet() {
  if (millis() - lastConnCheck < CONN_CHECK_INTERVAL) return;
  lastConnCheck = millis();

  bool wasOk = internetOk;
  internetOk = checkInternetNow();

  if (internetOk != wasOk) {
    Serial.print(F("Internet status changed: "));
    Serial.println(internetOk ? F("OK") : F("NO INTERNET (WiFi connected but no internet access)"));
    // Refresh the idle screen/state immediately so the change is
    // reflected right away rather than waiting for the next scan.
    if (strcmp(currentType, "READY") == 0) {
      goIdle();
    }
  }
}

// ================= Mega Link (status display + buttons) =================
// Uses the SAME physical UART as the USB debug Serial connection (this
// board only has one hardware UART). That means: when the Mega is wired
// up to RX/TX, the USB Serial Monitor and the Mega are sharing the same
// wire. All protocol messages to the Mega are prefixed with '@' so the
// Mega's parser can reliably pick them out of the debug text mixed in
// alongside them; incoming messages FROM the Mega (button presses) are
// parsed the same way here.

void sendToMega(const String& msg) {
  Serial.print('@');
  Serial.println(msg);
}

// Updates the persistent "current state" and sends it immediately.
// This is called on every real event (ready, processing, scan result).
void setCurrentState(const char* type, const char* name, const char* detail) {
  strncpy(currentType, type, FIELD_BUF_LEN - 1);
  currentType[FIELD_BUF_LEN - 1] = '\0';
  strncpy(currentName, name, FIELD_BUF_LEN - 1);
  currentName[FIELD_BUF_LEN - 1] = '\0';
  strncpy(currentDetail, detail, MESSAGE_BUF_LEN - 1);
  currentDetail[MESSAGE_BUF_LEN - 1] = '\0';

  broadcastCurrentState();
  lastHeartbeat = millis(); // reset the heartbeat clock on a real event
}

// Sends whatever the current state is, right now. Used both for real
// events AND for the periodic heartbeat repeat.
void broadcastCurrentState() {
  String msg = "STATUS,";
  msg += currentType;
  msg += ",";
  msg += currentName;
  msg += ",";
  msg += currentDetail;
  sendToMega(msg);
}

// Call every loop() iteration. Re-sends the current state every
// HEARTBEAT_INTERVAL ms regardless of whether anything changed --
// this way, if the Uno misses one message (reset/noise glitch), it
// self-corrects within ~1.5s instead of staying stuck on stale text
// until the next real event happens.
void runMegaHeartbeat() {
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    broadcastCurrentState();
  }
}

void sendStatusToMega(const AttendanceResult& r, const char* uid) {
  if (!r.requestOk) {
    setCurrentState("ERROR", "", "Retap card");
    return;
  }

  const char* detail;
  if (strcmp(r.type, "IN") == 0) {
    detail = r.time;
  } else if (strcmp(r.type, "OUT") == 0) {
    detail = r.hours;
  } else {
    detail = r.message;
  }

  setCurrentState(r.type, r.name, detail);
}

void sendSystemInfoToMega() {
  // Compose exactly two ready-to-display lines (16 chars fits the LCD)
  // instead of many small sub-fields -- simpler to parse reliably on
  // the Mega side, and there's no room to show more anyway.
  String line1 = (WiFi.status() == WL_CONNECTED) ? "WiFi:OK " : "WiFi:DOWN ";
  line1 += WiFi.RSSI();
  line1 += "dBm";

  String line2 = "Up:";
  line2 += (millis() / 1000);
  line2 += "s H:";
  line2 += (ESP.getFreeHeap() / 1024);
  line2 += "K";

  sendToMega("SYSINFO," + line1 + "," + line2);
}

// Non-blocking line reader for messages coming back from the Mega
// (button presses). Call this every loop() iteration.
void checkMegaMessages() {
  static char buf[32];
  static size_t idx = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      buf[idx] = '\0';
      if (idx > 0 && buf[0] == '@') {
        handleMegaMessage(buf + 1); // skip the leading '@'
      }
      idx = 0;
    } else if (c != '\r' && idx < sizeof(buf) - 1) {
      buf[idx++] = c;
    } else {
      idx = 0; // overflow/garbage line, reset
    }
  }
}

void handleMegaMessage(const char* msg) {
  if (strcmp(msg, "BTN,1") == 0) {
    Serial.println(F(">> Button 1: system info + QR"));
    oledShowSysInfoQR();       // QR on this board's own OLED
    sendSystemInfoToMega();    // readable text pushed to the LCD
  }
}


// uid is a plain char* (from readCardUID's fixed buffer) -- no String.

AttendanceResult callAttendanceAPI(const char* uid) {
  // The network (mobile hotspot) has real jitter -- occasional slow or
  // failed connection attempts are expected, not necessarily a bug.
  // One automatic retry absorbs most of these transient blips before
  // we show the user an error.
  AttendanceResult result = callAttendanceAPIOnce(uid);
  if (!result.requestOk) {
    Serial.println(F(">> First attempt failed, retrying once..."));
    delay(500);
    result = callAttendanceAPIOnce(uid);
  }
  return result;
}

AttendanceResult callAttendanceAPIOnce(const char* uid) {

  AttendanceResult result;
  result.requestOk = false;
  result.success = false;
  result.type[0] = '\0';
  result.name[0] = '\0';
  result.time[0] = '\0';
  result.hours[0] = '\0';
  result.message[0] = '\0';

  logHeapStatus(F("Before request"));

  unsigned long t0 = millis();

  // Force-close any lingering/stale sockets before starting a new
  // connection. Confirmed root cause of some GET failed: -1
  // (Connection Failed) errors: if the ESP tried to reuse a socket
  // whose remote side (Cloudflare) had already closed it, the ESP
  // stayed stuck waiting on a dead connection.
  WiFiClient::stopAll();

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  // CONFIRMED sweet spot: 1024 was too small (truncated Cloudflare's
  // certificate, forcing extra round-trips / stalls on higher-latency
  // networks). 16KB (BearSSL's default) was too big (fragmented the
  // ~80KB heap and caused reboots). 5120 bytes RX reliably fits
  // Cloudflare's full certificate in one shot without pressuring RAM.
  secureClient.setBufferSizes(5120, 512);
  secureClient.setTimeout(12000); // ms, a bit more headroom for slower networks

  char url[URL_BUF_LEN];
  snprintf(url, sizeof(url), "https://%s/?uid=%s&key=%s", HOST, uid, API_KEY);

  HTTPClient https;
  https.setTimeout(12000); // ms, matches the underlying client timeout
  bool began = https.begin(secureClient, url);

  if (!began) {
    Serial.println(F("Could not begin HTTPS connection to proxy."));
    secureClient.stop();
    return result;
  }

  // Confirmed fix for GET failed: -1 (Connection Failed): HTTPClient
  // tries to keep the connection alive by default, and Cloudflare was
  // treating the ESP8266's bare default headers as bot traffic and
  // slowing/blocking it. Forcing the socket closed after each request
  // and sending a normal browser-style User-Agent fixed both.
  https.setReuse(false);
  https.addHeader(F("Connection"), F("close"));
  https.addHeader(F("User-Agent"), F("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36"));

  unsigned long t1 = millis();
  Serial.print(F("[TIMING] begin() took: "));
  Serial.print(t1 - t0);
  Serial.println(F(" ms"));

  int httpCode = https.GET();

  unsigned long t2 = millis();
  Serial.print(F("[TIMING] GET request took: "));
  Serial.print(t2 - t1);
  Serial.println(F(" ms"));

  if (httpCode != HTTP_CODE_OK) {
    Serial.print(F("GET failed. HTTP code: "));
    Serial.println(httpCode);
    Serial.print(F("Error: "));
    Serial.println(https.errorToString(httpCode));

    https.end();
    secureClient.stop();
    logHeapStatus(F("After failed request"));
    return result;
  }

  // Read the response body once, copy immediately into a fixed buffer,
  // and let HTTPClient's internal String go out of scope right away.
  char payload[PAYLOAD_BUF_LEN];
  {
    String tmp = https.getString();
    tmp.toCharArray(payload, sizeof(payload));
  }
  https.end();
  secureClient.stop();

  unsigned long t3 = millis();
  Serial.print(F("[TIMING] TOTAL: "));
  Serial.print(t3 - t0);
  Serial.println(F(" ms"));

  logHeapStatus(F("After request"));

  Serial.println(F("Raw Response:"));
  Serial.println(payload);

  StaticJsonDocument<256> doc; // real responses are ~130 bytes
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print(F("JSON parse failed: "));
    Serial.println(error.c_str());
    return result;
  }

  result.requestOk = true;
  result.success = doc["success"] | false;

  strncpy(result.type,    doc["type"]    | "", FIELD_BUF_LEN - 1);
  strncpy(result.name,    doc["name"]    | "", FIELD_BUF_LEN - 1);
  strncpy(result.time,    doc["time"]    | "", FIELD_BUF_LEN - 1);
  strncpy(result.hours,   doc["hours"]   | "", FIELD_BUF_LEN - 1);
  strncpy(result.message, doc["message"] | "", MESSAGE_BUF_LEN - 1);

  return result;
}

// ================= Display + Buzzer helpers =================

void shortBeep() {
  tone(BUZZER_PIN, 2500, 80); // quick high chirp on tap -- just acknowledges the card was read
}

// ================= Icon-based OLED screens =================
// The LCD (secondary display) now shows full readable text -- name,
// greeting, time-of-day message, etc, with scrolling for long names.
// Since the OLED is small and sits right next to it, it's been
// switched to a purely graphical, glanceable role instead of
// duplicating the same text: idle/processing/success/error are shown
// as icons, not words. This makes the two displays complement each
// other instead of repeating the same thing.
//
// NOTE on "live" animation during Processing: the actual network
// wait (callAttendanceAPI) is a blocking call, so a continuously
// spinning animation *during* that wait isn't possible without a
// bigger restructure into a non-blocking state machine. What's here
// instead: a brief animated flourish when ENTERING each state
// (checkmark/cross draw themselves progressively), and a static
// icon while waiting. If a live spinner during the actual wait is
// wanted later, that's a separate, larger change -- happy to do it
// if useful.

// Simple expanding-circle pulse animation shown right at power-on --
// gives a quick visual sign of life before WiFi connection begins.
void oledBootPulse() {
  int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
  for (int r = 2; r <= 22; r += 3) {
    oled.clearDisplay();
    oled.drawCircle(cx, cy, r, SSD1306_WHITE);
    oled.display();
    delay(60);
  }
}

// ---------- Icon badge geometry (shared by wifi/processing/success/error) ----------
// All icon screens use the same bold "filled circle with a black
// cutout glyph" language -- like a Material Design filled status
// icon (check_circle / cancel), instead of thin single-pixel outlines.
const int BADGE_CX = SCREEN_WIDTH / 2;
const int BADGE_CY = SCREEN_HEIGHT / 2;
const int BADGE_R = 27;

// Draws a thick line by stacking several 1px-offset parallel lines --
// SSD1306/Adafruit_GFX has no native stroke-width, so this fakes a
// smooth, bold stroke instead of a hairline-thin single drawLine().
void oledThickLine(int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
  for (int t = 0; t < thickness; t++) {
    oled.drawLine(x0, y0 + t, x1, y1 + t, color);
    oled.drawLine(x0 + t, y0, x1 + t, y1, color);
  }
}

// Animated WiFi connecting icon, in the same filled-badge language as
// the other icons: a white disc with a black "signal" glyph (source
// dot + progressively lit arcs) cut into it. Kept snappy (no grow-in
// animation) since it's called every ~300ms during the connection
// retry loop; the final "connected" frame gets a badge grow-in for
// a satisfying flourish, matching the success/error icons.
void oledWifiConnectingFrame(int frame) {
  bool isConnectedFrame = (frame >= 3);

  if (isConnectedFrame) {
    oledBadgeGrowIn(); // flourish once, only on the "connected!" frame
  } else {
    oled.clearDisplay();
    oled.fillCircle(BADGE_CX, BADGE_CY, BADGE_R, SSD1306_WHITE);
  }

  int dotY = BADGE_CY + 10;
  oled.fillCircle(BADGE_CX, dotY, 3, SSD1306_BLACK); // signal source dot

  const int radii[3] = {8, 14, 20};
  int litArcs = frame; // 0..3
  for (int i = 0; i < 3 && i < litArcs; i++) {
    // Two adjacent radii per arc to fake a bold ~2px stroke; cornername
    // 0x03 = upper-left | upper-right quadrants -> arc opens downward.
    oled.drawCircleHelper(BADGE_CX, dotY, radii[i],     0x03, SSD1306_BLACK);
    oled.drawCircleHelper(BADGE_CX, dotY, radii[i] + 1, 0x03, SSD1306_BLACK);
  }

  oled.display();
}

// Idle icon: a simple card outline with a couple of "tap" wave arcs,
// no text -- shown whenever the kiosk is waiting for a scan.
void oledIdleScreen() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  int cardX = 30, cardY = 18, cardW = 40, cardH = 28;
  oled.drawRoundRect(cardX, cardY, cardW, cardH, 4, SSD1306_WHITE);
  oled.fillRoundRect(cardX + 5, cardY + 5, 10, 8, 2, SSD1306_WHITE); // chip

  // Wave arcs to the right of the card, suggesting "tap here"
  oled.drawCircle(cardX + cardW + 14, cardY + cardH / 2, 8, SSD1306_WHITE);
  oled.drawCircle(cardX + cardW + 14, cardY + cardH / 2, 14, SSD1306_WHITE);

  // Small warning badge in the corner if WiFi is connected but the
  // internet isn't actually reachable -- visible at a glance without
  // needing to tap a card and wait for it to fail.
  if (!internetOk) {
    int wx = SCREEN_WIDTH - 12, wy = 12;
    oled.fillCircle(wx, wy, 9, SSD1306_WHITE);
    oled.fillRect(wx - 1, wy - 5, 2, 6, SSD1306_BLACK);  // "!" stem
    oled.fillRect(wx - 1, wy + 3, 2, 2, SSD1306_BLACK);  // "!" dot
  }

  oled.display();
}

// Combines the idle icon + LCD state update into one call, so the
// two displays never drift out of sync (previously the OLED could go
// idle after a scan while the LCD kept re-broadcasting the old scan
// result via heartbeat indefinitely -- this fixes that).
void goIdle() {
  oledIdleScreen();
  setCurrentState("READY", "", internetOk ? "Tap your card" : "No Internet - Check WiFi");
}

// Animates the badge's white disc growing in from the center, over a
// handful of frames (~150ms total) -- a quick, polished "pop in"
// instead of the icon just appearing instantly.
void oledBadgeGrowIn() {
  for (int r = 5; r <= BADGE_R; r += 5) {
    oled.clearDisplay();
    oled.fillCircle(BADGE_CX, BADGE_CY, r, SSD1306_WHITE);
    oled.display();
    delay(16);
  }
  oled.clearDisplay();
  oled.fillCircle(BADGE_CX, BADGE_CY, BADGE_R, SSD1306_WHITE);
  oled.display();
}

// Static processing icon: a bold ring (donut) cut into the badge --
// a clean, universally-recognizable "loading" motif, much smoother
// at this resolution than the old thin hourglass outline. Shown once
// when entering PROCESSING (see earlier note on why this isn't a
// live spinner -- the network wait is a blocking call).
void oledProcessingScreen() {
  oledBadgeGrowIn();
  // Cut a smaller black circle out of the center, leaving a thin ring
  // -- slimmer than a first attempt (was too thick/heavy looking).
  oled.fillCircle(BADGE_CX, BADGE_CY, BADGE_R - 5, SSD1306_BLACK);
  oled.display();
}

// Draws the checkmark badge: white disc grows in, then a bold black
// checkmark is "cut" into it in two smooth animated strokes.
void oledDrawCheckmarkAnimated() {
  oledBadgeGrowIn();

  int x1 = BADGE_CX - 13, y1 = BADGE_CY + 1;
  int x2 = BADGE_CX - 4,  y2 = BADGE_CY + 11;
  int x3 = BADGE_CX + 15, y3 = BADGE_CY - 12;

  for (int t = 0; t <= 8; t++) {
    int ix = x1 + (x2 - x1) * t / 8;
    int iy = y1 + (y2 - y1) * t / 8;
    oledThickLine(x1, y1, ix, iy, 3, SSD1306_BLACK);
    oled.display();
    delay(16);
  }
  for (int t = 0; t <= 8; t++) {
    int ix = x2 + (x3 - x2) * t / 8;
    int iy = y2 + (y3 - y2) * t / 8;
    oledThickLine(x2, y2, ix, iy, 3, SSD1306_BLACK);
    oled.display();
    delay(16);
  }
}

// Draws the cross (X) badge: white disc grows in, then a bold black
// X is "cut" into it, both diagonals animating outward together from
// the center (each frame just extends further -- no need to redraw
// the disc since the shorter previous stroke is a subset of the new,
// longer one).
void oledDrawCrossAnimated() {
  oledBadgeGrowIn();

  int r = 15;
  for (int t = 0; t <= 10; t++) {
    int dx = (r * t) / 10, dy = (r * t) / 10;
    oledThickLine(BADGE_CX - dx, BADGE_CY - dy, BADGE_CX + dx, BADGE_CY + dy, 3, SSD1306_BLACK);
    oledThickLine(BADGE_CX - dx, BADGE_CY + dy, BADGE_CX + dx, BADGE_CY - dy, 3, SSD1306_BLACK);
    oled.display();
    delay(16);
  }
}

// Button 1 ("system info"): shows a QR code on this board's own OLED
// encoding live diagnostics as plain text (not a URL -- there's no
// hosted page/repo yet). Scanning it with any phone camera instantly
// shows WiFi status, signal strength, uptime, and free heap --
// genuinely useful for on-site troubleshooting without needing a
// laptop/Serial monitor. If a docs page or GitHub repo exists later,
// this can easily be swapped to encode a URL instead.
void oledShowSysInfoQR() {
  char text[64];
  snprintf(text, sizeof(text), "WiFi:%s|%ddBm|Up:%lus|Heap:%uK|v8.3",
           (WiFi.status() == WL_CONNECTED) ? "OK" : "DOWN",
           WiFi.RSSI(),
           millis() / 1000,
           ESP.getFreeHeap() / 1024);

  const uint8_t qrVersion = 3; // 29x29 modules -- comfortably fits our text
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, text);

  int moduleSize = 2; // 29 modules * 2px = 58px, fits within the 64px-tall OLED
  int qrPixels = qrcode.size * moduleSize;
  int offsetX = (SCREEN_WIDTH - qrPixels) / 2;
  int offsetY = (SCREEN_HEIGHT - qrPixels) / 2;

  oled.clearDisplay();
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        oled.fillRect(offsetX + x * moduleSize, offsetY + y * moduleSize,
                      moduleSize, moduleSize, SSD1306_WHITE);
      }
    }
  }
  oled.display();

  delay(5000); // give time to scan, then return to idle (button presses
               // are manual/occasional, so a brief block here is fine)
  goIdle();
}

void successBeep() {
  // Ascending 3-note "ta-da" chime (G - C - E), much more pleasant
  // and distinct than the old flat two-tone beep.
  tone(BUZZER_PIN, 1568, 100); // G6
  delay(110);
  tone(BUZZER_PIN, 2093, 100); // C7
  delay(110);
  tone(BUZZER_PIN, 2637, 180); // E7
  delay(180);
}

void errorBeep() {
  // Two short low buzzes, clearly different in rhythm and pitch from
  // the success chime -- easy to distinguish by ear without looking
  // at the screen (e.g. "already done" / "invalid card" / connection
  // errors all use this).
  tone(BUZZER_PIN, 300, 150);
  delay(200);
  tone(BUZZER_PIN, 300, 150);
  delay(150);
}

// ================= RFID =================
// Writes into a caller-provided fixed buffer instead of returning a
// String. Returns true if a UID was read.

bool readCardUID(char* outUid, size_t outUidLen) {
  outUid[0] = '\0';

  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial()) return false;

  size_t pos = 0;
  for (byte i = 0; i < rfid.uid.size && pos + 2 < outUidLen; i++) {
    snprintf(outUid + pos, outUidLen - pos, "%02X", rfid.uid.uidByte[i]);
    pos += 2;
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return true;
}

// ================= React to result =================

void handleResult(const AttendanceResult& r, const char* uid) {

  sendStatusToMega(r, uid);

  if (!r.requestOk) {
    Serial.print(F(">> Request failed (proxy/network issue). Card: "));
    Serial.println(uid);
    errorBeep();
    oledDrawCrossAnimated();
    delay(1800);
    goIdle();
    return;
  }

  Serial.println(F("---- Result ----"));
  Serial.print(F("type: "));
  Serial.println(r.type);

  if (strcmp(r.type, "IN") == 0) {
    Serial.print(F(">> Welcome "));
    Serial.print(r.name);
    Serial.print(F(", IN marked at "));
    Serial.println(r.time);
    successBeep();
    oledDrawCheckmarkAnimated();

  } else if (strcmp(r.type, "OUT") == 0) {
    Serial.print(F(">> Goodbye "));
    Serial.print(r.name);
    Serial.print(F(", worked "));
    Serial.println(r.hours);
    successBeep();
    oledDrawCheckmarkAnimated();

  } else if (strcmp(r.type, "ALREADY_OUT") == 0) {
    Serial.print(F(">> "));
    Serial.print(r.name);
    Serial.print(F(": "));
    Serial.println(r.message);
    errorBeep();
    oledDrawCrossAnimated();

  } else if (strcmp(r.type, "INVALID") == 0) {
    Serial.print(F(">> Unrecognized card: "));
    Serial.println(uid);
    errorBeep();
    oledDrawCrossAnimated();

  } else if (strcmp(r.type, "INACTIVE") == 0) {
    Serial.print(F(">> "));
    Serial.print(r.name);
    Serial.print(F(": "));
    Serial.println(r.message);
    errorBeep();
    oledDrawCrossAnimated();

  } else {
    Serial.print(F(">> Unknown response type, message: "));
    Serial.println(r.message);
    errorBeep();
    oledDrawCrossAnimated();
  }

  delay(1800); // let the icon sit on screen briefly (LCD shows the full text detail)
  goIdle();
}

// ================= Main =================

void setup() {
  Serial.begin(9600);   // lowered from 115200 -- much more tolerant of
                         // breadboard wiring noise on the Mega link
  delay(200);
  Serial.println();
  Serial.println(F("=== Attendance System: Full Flow (v8.5 - Backup WiFi) ==="));

  SPI.begin();
  rfid.PCD_Init();

  pinMode(BUZZER_PIN, OUTPUT);

  if (!oled.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("OLED not found! Check SPI wiring (CS/DC/RES)."));
  } else {
    oledBootPulse();
  }

  connectWiFi(); // shows its own animated WiFi icon while connecting

  Serial.println(F("Ready. Tap a card..."));
  goIdle();

  logHeapStatus(F("At boot"));

  // Start the software watchdog: checks every 1s whether loop() has
  // updated its heartbeat recently; force-restarts if not.
  lastLoopRunTime = millis();
  watchdogTicker.attach_ms(1000, checkWatchdog);
}

void loop() {

  lastLoopRunTime = millis(); // feed the watchdog -- proves loop() is alive

  checkMegaMessages(); // non-blocking, handles Button 1 / Button 2 requests
  runMegaHeartbeat();  // re-sends current state every ~1.5s so a missed
                        // message self-corrects instead of leaving the
                        // Uno stuck on stale text
  maybeCheckInternet(); // periodic (every 30s) WiFi-connected-but-no-
                        // internet detection; only actually probes
                        // when due, bounded to a 3s timeout

  char uid[UID_BUF_LEN];
  if (!readCardUID(uid, sizeof(uid))) return; // no card tapped, keep looping

  if (millis() - lastScanTime < SCAN_COOLDOWN) {
    return;
  }
  lastScanTime = millis();

  Serial.println();
  Serial.print(F("Card detected: "));
  Serial.println(uid);

  shortBeep();
  oledProcessingScreen();
  setCurrentState("PROCESSING", "", "Please wait");

  ensureWiFi();

  AttendanceResult result = callAttendanceAPI(uid);
  handleResult(result, uid);
}
