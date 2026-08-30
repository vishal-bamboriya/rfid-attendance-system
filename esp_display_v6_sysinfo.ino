/**
 * ==========================================
 * RFID ATTENDANCE SYSTEM - Secondary Display (ESP8266 version) v2.0
 * UX POLISH PASS
 * ==========================================
 * v1.0 -> basic LCD status display + button + LED, linked to main
 *         NodeMCU over direct 3.3V UART (no voltage divider needed).
 * v2.0 (this file) -> visual/UX improvements:
 *   - Custom LCD icons (checkmark, cross) shown next to status text
 *     instead of plain words alone.
 *   - Animated "Processing..." dots that tick locally on this board
 *     (no extra messages needed from the main board) so it feels
 *     alive while waiting, not frozen.
 *   - Startup splash screen sequence at boot.
 *   - Time-of-day greeting on IN (Good Morning/Afternoon/Evening/
 *     Night) instead of a generic "Welcome!", using the IN time
 *     the main board sends.
 *
 * WIRING / PROTOCOL: unchanged from v1.0 -- see original comments
 * preserved below.
 *
 * WIRING (both boards are 3.3V logic -- direct wires, no divider):
 *   Main NodeMCU TX (D10) -----------------------> This ESP RX
 *   Main NodeMCU RX (D9)  <----------------------- This ESP TX
 *   Main NodeMCU GND      ------------------------ This ESP GND
 *                                                   (dedicated, direct wire)
 *
 * PROTOCOL (newline-terminated lines, always starting with '@'):
 *   Main NodeMCU -> This board:
 *     @STATUS,<type>,<name>,<detail>
 *       type: READY | PROCESSING | IN | OUT | ALREADY_OUT | INVALID
 *             | INACTIVE | ERROR
 *   This board -> Main NodeMCU (sent when the button is pressed):
 *     @BTN,1
 *   Main NodeMCU -> This board (reply):
 *     @LOG,<type>,<name>,<detail>
 *
 * LCD WIRING (16x2 parallel HD44780-compatible -- confirmed working
 * with a JHD162A module, but works with any 16x2 HD44780 LCD):
 *   RS -> D1   E -> D2   D4 -> D3   D5 -> D4   D6 -> D5   D7 -> D6
 *   RW -> GND  V0 -> wiper of a 10K contrast pot
 *
 * BUTTON (active LOW, internal pull-up -- wire between pin and GND):
 *   Button 1 (system info + QR) -> D7
 *
 * LED:
 *   Red (error/rejected only) -> D8   (with ~220ohm resistor to GND)
 */

#include <LiquidCrystal.h>

// ---------- LCD ----------
const int LCD_RS = D1, LCD_E = D2, LCD_D4 = D3, LCD_D5 = D4, LCD_D6 = D5, LCD_D7 = D6;
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ---------- Button ----------
const int BTN1_PIN = D7; // system info + QR (main board shows QR on its OLED, sends text here for LCD)
const unsigned long DEBOUNCE_MS = 250;
unsigned long lastBtn1Time = 0;

// ---------- LED ----------
const int LED_RED_PIN = D8;

// ================= Custom LCD icons =================
// HD44780 supports 8 custom characters (slots 0-7), 5x8 pixels each.
// Defined once in setup() via lcd.createChar(), then used in text
// like a normal character: lcd.write(byte(0)).
#define ICON_CHECK 0
#define ICON_CROSS 1

byte iconCheckBitmap[8] = {
  B00000,
  B00001,
  B00011,
  B10110,
  B11100,
  B01000,
  B00000,
  B00000
};

byte iconCrossBitmap[8] = {
  B00000,
  B11011,
  B01110,
  B00100,
  B01110,
  B11011,
  B00000,
  B00000
};

// ---------- State ----------
unsigned long tempScreenUntil = 0;
const unsigned long TEMP_SCREEN_DURATION = 4000; // ms

// The last "real" status (not a temp log/lookup screen), used to
// restore the screen after a temp screen expires, and to drive the
// processing-dots animation.
String currentType = "READY";
String lastLine1 = "Ready";
String lastLine2 = "Tap your card";

// Tracks what's actually drawn right now, to avoid needless LCD
// rewrites (which take a few ms and can flicker).
String shownLine1 = "";
String shownLine2 = "";

// Tracks whether we've received at least one real message from the
// main board yet. Until then, the LCD shouldn't claim "Ready" --
// that would be a guess, not a confirmed state.
bool haveReceivedAnyMessage = false;

// ---------- Processing animation ----------
unsigned long lastAnimTick = 0;
const unsigned long ANIM_INTERVAL = 400; // ms per animation frame
int animFrame = 0;

// ---------- Scrolling (for names/text longer than 16 chars) ----------
// Only line 2 needs this in practice (names) -- line 1 is always a
// short fixed label ("Welcome!", "Goodbye!", etc).
String scrollFullText = "";     // the full un-truncated text, if too long
bool scrollActive = false;
int scrollPos = 0;
unsigned long lastScrollTick = 0;
const unsigned long SCROLL_INTERVAL = 400; // ms per scroll step
const unsigned long SCROLL_PAUSE = 1000;   // ms pause at start/end of each cycle
unsigned long scrollPauseUntil = 0;

// ---------- Forward declarations ----------
// Arduino IDE auto-generates function prototypes above this point at
// compile time, but those auto-generated prototypes DON'T carry over
// default argument values -- causing "too few arguments" errors at
// every call site that relies on the defaults. Declaring these
// explicitly here (with the defaults) fixes it; the real definitions
// further down must NOT repeat the defaults.
void showLine(const String& line1, const String& line2, bool checkIcon = false, bool crossIcon = false);
void showTemp(const String& line1, const String& line2, bool checkIcon = false, bool crossIcon = false);

void setup() {
  // Same UART used for both USB debug AND the link to the main
  // NodeMCU (this board's single hardware UART). 9600 baud matches
  // the main board's link speed.
  Serial.begin(9600);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);

  lcd.begin(16, 2);
  lcd.createChar(ICON_CHECK, iconCheckBitmap);
  lcd.createChar(ICON_CROSS, iconCrossBitmap);

  playStartupSplash();

  // Don't assume "Ready" yet -- we haven't actually heard from the
  // main board. Show a neutral waiting state until the first real
  // message arrives.
  lcd.clear();
  centerText("Waiting for", 0);
  centerText("Main Board...", 1);
}

void loop() {
  readButton();
  readFromMainBoard();
  updateProcessingAnimation();
  updateScroll();

  if (tempScreenUntil != 0 && millis() > tempScreenUntil) {
    tempScreenUntil = 0;
    showLine(lastLine1, lastLine2);
  }
}

// ================= Startup splash =================
// Blocking is fine here -- nothing else needs to run yet, the board
// has just powered on. Keeps total splash time short (~2.5s) so it
// doesn't delay real status once the main board starts sending.
void playStartupSplash() {
  lcd.clear();
  centerText("RFID Attendance", 0);
  centerText("System", 1);
  delay(1200);

  for (int i = 0; i < 4; i++) {
    lcd.clear();
    centerText("Initializing", 0);
    lcd.setCursor(0, 1);
    String dots = "";
    for (int d = 0; d <= (i % 4); d++) dots += ".";
    centerText(dots, 1);
    delay(300);
  }
}

void centerText(const String& text, int row) {
  int len = text.length();
  int col = (16 - len) / 2;
  if (col < 0) col = 0;
  lcd.setCursor(col, row);
  lcd.print(text);
}

// ================= Processing animation =================
// Ticks locally every ANIM_INTERVAL ms whenever the current state is
// PROCESSING, cycling the dot count on line 2. Uses setCursor + a
// fixed-width field (not lcd.clear()) so it doesn't flicker.
void updateProcessingAnimation() {
  if (currentType != "PROCESSING") return;
  if (millis() - lastAnimTick < ANIM_INTERVAL) return;

  lastAnimTick = millis();
  animFrame = (animFrame + 1) % 4; // 0..3 dots

  String dots = "";
  for (int i = 0; i < animFrame; i++) dots += ".";

  String line2 = "Processing" + dots;
  // Pad with spaces to fully overwrite any leftover dots from a
  // longer previous frame, without a full lcd.clear() (which flickers).
  while (line2.length() < 16) line2 += " ";

  lcd.setCursor(0, 1);
  lcd.print(line2);
  shownLine2 = line2; // keep shownLine2 in sync so showLine() dedup logic stays correct
}

// ================= Button =================

void readButton() {
  if (digitalRead(BTN1_PIN) == LOW && millis() - lastBtn1Time > DEBOUNCE_MS) {
    lastBtn1Time = millis();
    sendToMainBoard("BTN,1");
  }
}

void sendToMainBoard(const String& msg) {
  Serial.print('@');
  Serial.println(msg);
}

// ================= Receiving from main NodeMCU =================

void readFromMainBoard() {
  static String buf = "";

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      buf.trim();
      if (buf.length() > 0 && buf[0] == '@') {
        handleMessage(buf.substring(1)); // strip leading '@'
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
      if (buf.length() > 120) buf = ""; // guard against garbage/overflow
    }
  }
}

int splitFields(const String& msg, String* fields, int maxFields) {
  int count = 0;
  int start = 0;
  while (count < maxFields) {
    int comma = msg.indexOf(',', start);
    if (comma == -1) {
      fields[count++] = msg.substring(start);
      break;
    }
    fields[count++] = msg.substring(start, comma);
    start = comma + 1;
  }
  return count;
}

void handleMessage(const String& msg) {
  haveReceivedAnyMessage = true;

  String fields[4];
  int n = splitFields(msg, fields, 4);
  if (n == 0) return;

  String kind = fields[0]; // STATUS, LOG, or SYSINFO

  if (kind == "STATUS" || kind == "LOG") {
    String type   = n > 1 ? fields[1] : "";
    String name   = n > 2 ? fields[2] : "";
    String detail = n > 3 ? fields[3] : "";
    renderStatus(kind, type, name, detail);

  } else if (kind == "SYSINFO") {
    // Already display-ready two lines from the main board (see
    // sendSystemInfoToMega() there) -- shown as a temp overlay, same
    // as the old "previous log" screen used to be.
    String line1 = n > 1 ? fields[1] : "";
    String line2 = n > 2 ? fields[2] : "";
    showTemp(line1, line2);
  }
}

// Turns an HH:MM:SS (or HH:MM) time string into a time-of-day
// greeting. Falls back to a generic greeting if parsing fails.
String greetingForTime(const String& timeStr) {
  if (timeStr.length() < 2) return "Welcome!";

  int hour = timeStr.substring(0, 2).toInt();

  if (hour >= 5 && hour < 12)  return "Good Morning!";
  if (hour >= 12 && hour < 17) return "Good Afternoon!";
  if (hour >= 17 && hour < 21) return "Good Evening!";
  return "Good Night!";
}

void renderStatus(const String& kind, const String& type, const String& name, const String& detail) {
  digitalWrite(LED_RED_PIN, LOW);
  String previousType = currentType;
  currentType = type;

  // Repeat PROCESSING heartbeats (same state as last time) shouldn't
  // trigger a full lcd.clear() -- that would reset/flicker the local
  // dot animation every ~1.5s. The animation ticker already owns
  // line 2; line 1 ("Please wait") was already drawn on the first
  // transition into this state.
  if (kind == "STATUS" && type == "PROCESSING" && previousType == "PROCESSING") {
    return;
  }

  String line1, line2;
  bool useCheckIcon = false;
  bool useCrossIcon = false;

  if (type == "READY") {
    line1 = "Ready";
    line2 = "Tap your card";

  } else if (type == "PROCESSING") {
    line1 = "Please wait";
    line2 = "Processing";
    animFrame = 0; // restart the dot animation fresh each time we enter this state

  } else if (type == "IN") {
    useCheckIcon = true;
    line1 = greetingForTime(detail);
    line2 = name; // untruncated -- showLine() will scroll it if too long

  } else if (type == "OUT") {
    useCheckIcon = true;
    line1 = "Goodbye!";
    line2 = name;

  } else if (type == "ALREADY_OUT") {
    useCrossIcon = true;
    digitalWrite(LED_RED_PIN, HIGH);
    line1 = "Already Done";
    line2 = name;

  } else if (type == "INVALID") {
    useCrossIcon = true;
    digitalWrite(LED_RED_PIN, HIGH);
    line1 = "Unknown Card";
    line2 = "Not Registered";

  } else if (type == "INACTIVE") {
    useCrossIcon = true;
    digitalWrite(LED_RED_PIN, HIGH);
    line1 = "Inactive";
    line2 = name;

  } else if (type == "ERROR") {
    useCrossIcon = true;
    digitalWrite(LED_RED_PIN, HIGH);
    line1 = "Connection";
    line2 = "Error - Retap";

  } else if (type == "NONE") {
    line1 = "No Previous";
    line2 = "Log Yet";

  } else {
    line1 = "Status:";
    line2 = type;
  }

  if (kind == "LOG") {
    showTemp(line1, line2, useCheckIcon, useCrossIcon);
  } else {
    lastLine1 = line1;
    lastLine2 = line2;
    tempScreenUntil = 0;
    showLine(line1, line2, useCheckIcon, useCrossIcon);
  }
}

// ================= LCD helpers =================

String truncate16(const String& s) {
  if (s.length() <= 16) return s;
  return s.substring(0, 16);
}

// line1 optionally gets a checkmark/cross icon prefix (takes 1 char +
// 1 space, so text is truncated to 14 chars in that case to still
// fit in 16 columns). line2 is NOT truncated -- if it's longer than
// 16 chars (e.g. a long name), it scrolls instead of getting cut off.
void showLine(const String& line1, const String& line2, bool checkIcon, bool crossIcon) {
  String t1 = checkIcon || crossIcon ? truncate14(line1) : truncate16(line1);

  // Include the icon flag in the dedup key so switching between,
  // say, two different IN events with the same name still redraws.
  String key1 = (checkIcon ? "C:" : crossIcon ? "X:" : "-:") + t1;
  bool sameLine1 = (key1 == shownLine1);
  bool sameLine2Source = (line2 == scrollFullText || (!scrollActive && line2 == shownLine2));
  if (sameLine1 && sameLine2Source) return;

  shownLine1 = key1;

  lcd.clear();
  lcd.setCursor(0, 0);
  if (checkIcon) {
    lcd.write(byte(ICON_CHECK));
    lcd.print(' ');
  } else if (crossIcon) {
    lcd.write(byte(ICON_CROSS));
    lcd.print(' ');
  }
  lcd.print(t1);

  setLine2Text(line2);
}

// Sets up line 2, either as static text (fits in 16 chars) or as a
// scrolling marquee (longer text, e.g. a long staff name).
void setLine2Text(const String& text) {
  if (text.length() <= 16) {
    scrollActive = false;
    scrollFullText = "";
    shownLine2 = text;
    lcd.setCursor(0, 1);
    lcd.print(text);
    // Pad remainder with spaces in case a previous scroll left stray chars
    for (int i = text.length(); i < 16; i++) lcd.print(' ');
    return;
  }

  // Text is too long -- scroll it. Add a gap before it repeats so the
  // wrap-around reads cleanly instead of running two copies together.
  scrollFullText = text;
  scrollActive = true;
  scrollPos = 0;
  scrollPauseUntil = millis() + SCROLL_PAUSE; // brief pause showing the start before scrolling begins
  drawScrollFrame();
}

void drawScrollFrame() {
  String padded = scrollFullText + "    "; // 4-space gap before it repeats
  String window = "";
  for (int i = 0; i < 16; i++) {
    window += padded[(scrollPos + i) % padded.length()];
  }
  lcd.setCursor(0, 1);
  lcd.print(window);
}

// Advances the scroll position at a steady interval, with a short
// pause at the start of each cycle so the beginning of the name is
// readable before it starts moving.
void updateScroll() {
  if (!scrollActive) return;
  if (millis() < scrollPauseUntil) return;
  if (millis() - lastScrollTick < SCROLL_INTERVAL) return;

  lastScrollTick = millis();
  String padded = scrollFullText + "    ";
  scrollPos = (scrollPos + 1) % padded.length();
  drawScrollFrame();

  if (scrollPos == 0) {
    scrollPauseUntil = millis() + SCROLL_PAUSE; // pause again at the loop point
  }
}

String truncate14(const String& s) {
  if (s.length() <= 14) return s;
  return s.substring(0, 14);
}

void showTemp(const String& line1, const String& line2, bool checkIcon, bool crossIcon) {
  showLine(line1, line2, checkIcon, crossIcon);
  tempScreenUntil = millis() + TEMP_SCREEN_DURATION;
}
