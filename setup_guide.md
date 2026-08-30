# RFID Attendance System — Setup Guide (Simple Steps)

Follow these steps in order. Don't skip ahead — later steps need things from earlier steps.

---

## What You'll Need

- 2x NodeMCU (ESP8266) boards
- 1x MFRC522 RFID reader + a few RFID cards
- 1x 0.96" OLED display (SPI, SSD1306)
- 1x 16x2 LCD (JHD162A or similar, HD44780-compatible)
- 1x Passive buzzer
- 1x Push button
- 1x LED + a ~220Ω resistor
- 1x 10kΩ potentiometer (for LCD contrast)
- A Google account
- A Cloudflare account (free — [cloudflare.com](https://cloudflare.com))
- Arduino IDE installed on your computer

---

## Step 1 — Set Up the Google Sheet

1. Go to [sheets.google.com](https://sheets.google.com) and create a new blank spreadsheet.
2. Rename it to something like "Attendance Data".
3. Create a sheet (tab) named exactly **`Staff`** with these column headers in row 1:
   ```
   UID | Staff ID | Name | Department | Designation | Status
   ```
4. Fill in your staff members. The **UID** column should have each person's RFID card ID (you'll get this later when you scan a card — for now, leave it blank or fill in later).
5. Create another sheet (tab) named exactly **`Settings`** with:
   ```
   Key           | Value
   API Key       | (make up any random password-like text, e.g. Xk9mP2vQ7z)
   Time Zone     | Asia/Kolkata   (or your own timezone)
   ```
   **Remember this API Key — you'll need it again in Step 3 and Step 5.**

---

## Step 2 — Set Up the Backend (Google Apps Script)

1. In your Google Sheet, click **Extensions → Apps Script**.
2. This opens a code editor. Delete anything in the default file.
3. Create 6 files (use the **+** next to "Files" → "Script") named exactly:
   - `Code.gs`
   - `Config.gs`
   - `Staff.gs`
   - `Attendance.gs`
   - `Utils.gs`
   - `TestFlow.gs`
4. Copy-paste the matching content into each file (from the project files you have).
5. Click **Deploy → New deployment**.
   - Click the gear icon next to "Select type" → choose **Web app**.
   - "Execute as": **Me**
   - "Who has access": **Anyone** (important — not "Anyone with Google account")
   - Click **Deploy**.
6. Copy the URL it gives you (looks like `https://script.google.com/macros/s/AKfycb.../exec`).
7. From that URL, copy just the middle part — between `/macros/s/` and `/exec`. This is your **Script ID**. Save it somewhere, you'll need it in Step 3.

---

## Step 3 — Set Up the Cloudflare Worker (Speeds Everything Up)

1. Go to [dash.cloudflare.com](https://dash.cloudflare.com) and sign up (free, no credit card needed).
2. Click **Workers & Pages → Create Application → Create Worker**.
3. Give it a name (e.g. `attendance-proxy`) → **Deploy**.
4. Click **Edit Code**. Delete everything, paste in the `worker.js` file content.
5. Click **Deploy** again.
6. Go to **Settings → Variables** on this Worker.
7. Add a variable:
   - Name: `GSCRIPT_ID`
   - Value: the Script ID you saved in Step 2.
8. Save.
9. Note down your Worker's URL — it'll look like `https://attendance-proxy.yourname.workers.dev`. You'll need this in Step 5.

---

## Step 4 — Wire Up the Hardware

Follow the separate `wiring_and_files.md` document for exact pin connections for both boards (RFID, OLED, LCD, buzzer, button, LED, and the link between the two boards).

---

## Step 5 — Set Up the Firmware

### Main board firmware
1. Open `attendance_firmware_v85_backupwifi.ino` in Arduino IDE.
2. Near the top, fill in:
   ```cpp
   const char* WIFI_SSID     = "your_wifi_name";
   const char* WIFI_PASSWORD = "your_wifi_password";
   const char* HOST = "your-worker-name.workers.dev";  // from Step 3
   const char* API_KEY = "your_api_key";                 // from Step 1
   ```
3. Install these libraries first (Arduino IDE → Sketch → Include Library → Manage Libraries — search and install each):
   - MFRC522
   - ArduinoJson
   - Adafruit GFX Library
   - Adafruit SSD1306
   - QRCode (by Richard Moore)
4. Select your board: **Tools → Board → NodeMCU 1.0 (ESP-12E Module)**.
5. Plug in the main board via USB, select the right Port, click **Upload**.

### Secondary display board firmware
1. Open `esp_display_v6_sysinfo.ino` in Arduino IDE.
2. Install the `LiquidCrystal` library if not already there (usually pre-installed).
3. Plug in the second board, select its Port, click **Upload**.

---

## Step 6 — Test It

1. Power on both boards.
2. The main board's OLED should show a WiFi-connecting animation, then an idle "tap card" icon.
3. Tap a registered card:
   - Should show a checkmark and mark **IN**.
   - LCD should show a greeting and the person's name.
4. Tap the same card again:
   - Should mark **OUT** and show hours worked.
5. Tap an unregistered card:
   - Should show an error (cross icon), "Unknown Card".
6. Press the button on the secondary display board:
   - Should show a QR code on the main OLED, and text info on the LCD.

If a scan doesn't work, open the Arduino IDE's **Serial Monitor** (9600 baud) on the main board to see error messages.

---

## Common Problems

| Problem | Likely Fix |
|---|---|
| OLED shows nothing | Check wiring, especially CS/DC/RES pins |
| LCD shows blank or black boxes | Adjust the contrast potentiometer |
| "UNAUTHORIZED" error | API Key in firmware doesn't match the Settings sheet |
| Card shows "INVALID" even though registered | Double-check the UID in the Staff sheet matches exactly (case-sensitive) |
| Very slow response (10+ seconds) | Check that firmware is calling your Worker URL, not Google directly |
| Secondary display stuck on "Waiting for Main Board" | Check the TX/RX/GND wiring between the two boards |

---

*That's it — once all 6 steps are done, the kiosk should be fully working.*
