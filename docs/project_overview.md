# RFID Staff Attendance System — Project Overview

A complete technical summary of everything used in this project: hardware, software, backend, and architecture.

---

## 1. What the System Does

A staff attendance kiosk where employees tap an RFID card to mark IN or OUT. Data is written automatically to Google Sheets, organized month by month. Two displays give real-time feedback (a small OLED for quick icons, a 16x2 LCD for detailed text), with buzzer and LED feedback alongside.

**Logic:**
- 1st scan of the day → marks **IN**
- 2nd scan → marks **OUT**, calculates working hours
- 3rd scan → rejected as **ALREADY_OUT**
- Unregistered card → **INVALID**

---

## 2. Hardware Used

### Main Board — NodeMCU (ESP8266)
The "brain" of the system. Reads RFID cards, talks to the backend over WiFi/HTTPS, drives the OLED and buzzer, and talks to the secondary display board over a direct UART link.

### Secondary Display Board — a second NodeMCU (ESP8266)
Dedicated to the 16x2 LCD, one button, and an error LED. Talks to the main board over a 3.3V-to-3.3V UART link (no voltage divider needed since both boards share the same logic level — this was a deliberate choice after an Arduino Uno/Mega version had reliability problems caused by the 5V↔3.3V voltage divider circuit).

### RFID Reader — MFRC522
SPI-connected card reader. Shares the hardware SPI bus with the OLED (same SCK/MOSI lines, separate chip-select pins).

### OLED Display — 0.96" SSD1306 (128x64, SPI)
Small, icon-only display: shows bold "filled badge" style graphics (a filled circle with the icon shape cut out in black) for idle/processing/success/error states, plus an animated WiFi-connecting icon, plus an on-demand QR code with diagnostics.

### LCD Display — 16x2 JHD162A (parallel HD44780-compatible)
Shows full readable text: greetings, staff names (auto-scrolls if too long to fit), time worked, error messages, and (on button press) system diagnostics as text.

### Buzzer
Passive piezo buzzer for audio feedback — a distinct 3-note ascending chime for success, a double low buzz for errors, and a quick chirp to acknowledge a tap.

### Buttons & LED (on the secondary display board)
- 1 button: shows system diagnostics (WiFi status, signal strength, uptime, free memory) as a QR code on the OLED and as text on the LCD
- 1 LED: lights up red on errors/rejections only

### Supporting components
- **Voltage divider** (1kΩ / 2kΩ resistors) — was used during Uno/Mega experimentation to step 5V down to 3.3V; no longer needed in the current two-ESP8266 design
- **Decoupling capacitors** (470µF near NodeMCU's 3.3V rail, smaller ones near the LCD backlight) — smooth out power spikes, mainly caused by the WiFi radio's current draw, which were causing intermittent resets
- **10kΩ potentiometer** — LCD contrast adjustment

---

## 3. Software & Backend

### Google Apps Script
Runs on Google's servers, receives HTTPS requests from the NodeMCU, and writes attendance records to Google Sheets. Creates one sheet per month (e.g. `Attendance_July_2026`). Handles the IN/OUT/ALREADY_OUT/INVALID logic and working-hours calculation.

### Google Sheets
The actual data store — one spreadsheet with a new sheet tab per month. A separate "Staff"/Settings sheet holds registered card UIDs, names, and the API key used to authenticate requests.

### Cloudflare Worker (proxy)
Sits between the NodeMCU and Google Apps Script. Google's Apps Script endpoint always issues an HTTP redirect to fetch its real response, which required the power-limited ESP8266 to do two full TLS handshakes — taking 10–30 seconds per scan. The Cloudflare Worker follows that redirect internally (on much faster infrastructure) and returns a single clean response, cutting scan time down to roughly 3–6 seconds.

---

## 4. Firmware (Arduino/C++)

### Main NodeMCU firmware — key libraries
- `MFRC522` — RFID reader driver
- `WiFiClientSecure` + `ESP8266HTTPClient` — HTTPS calls to the Cloudflare Worker
- `ArduinoJson` — parsing the JSON response from the backend
- `Adafruit_GFX` + `Adafruit_SSD1306` — OLED graphics
- `Ticker` — drives the software watchdog timer
- `QRCode` (by Richard Moore) — generates the on-screen QR code

### Secondary display firmware — key libraries
- `LiquidCrystal` — drives the 16x2 LCD
- `SoftwareSerial` — the link to the main board (the ESP8266's one hardware UART is shared with USB debug, so SoftwareSerial keeps both available)

### Notable firmware features
- **Heartbeat protocol** — the main board continuously re-broadcasts its current status (not just on change), so if one message is missed by the secondary display, it self-corrects within ~1.5 seconds instead of getting stuck
- **Software watchdog** — restarts the main board automatically if it ever hangs for 40+ seconds (safety net against rare/unknown library-level freezes)
- **Automatic retry** — one silent retry on a failed network request before showing an error, since some failures are just momentary network jitter
- **Backup WiFi network** — a second SSID/password can be configured; the board alternates between primary and backup every 15 seconds if the current one won't connect
- **Proactive "no internet" detection** — periodically pings a lightweight external endpoint (separate from the WiFi "connected" status) to catch cases where WiFi is connected but there's no actual internet access, without waiting for a scan to fail
- **RAM/stability fixes** — fixed-size buffers instead of Arduino `String` concatenation in the hot path, tuned TLS buffer sizes, disabled WiFi modem-sleep — all addressed real memory leaks and intermittent reset issues found during testing

---

## 5. Architecture Diagram (text form)

```
[RFID Card] --tap--> [Main NodeMCU] --HTTPS--> [Cloudflare Worker] --HTTPS--> [Google Apps Script] --> [Google Sheets]
                           |
                           | (direct 3.3V UART, both directions)
                           v
                  [Secondary NodeMCU] --> [16x2 LCD] + [Button] + [Error LED]
                           |
                    [Main NodeMCU also drives:]
                    [OLED] + [Buzzer]
```

---

## 6. Known Limitations

- The OLED cannot display Hindi/Devanagari text — it's a hardware font limitation of standard character LCDs/OLEDs, not a firmware bug. Staff names need to be in Roman/English script in the Google Sheet.
- There's no live spinning animation on the OLED during the actual network wait — the request is a blocking call, so what's shown is a static icon rather than a continuously animated one. A live version would need a larger non-blocking rewrite.
- The secondary display's debug Serial Monitor becomes unusable once the board is wired to the main board (both use the same single hardware UART) — SoftwareSerial handles the real link, so this only affects manual debugging, not normal operation.

---

*This document reflects the system as of the current firmware versions (main board v8.5, secondary display v6).*
