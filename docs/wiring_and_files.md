# RFID Attendance System — Final Files & Wiring Sequence (Beta 3.0)

---

## 1. Files

| File | Goes on | Purpose |
|---|---|---|
| `attendance_firmware_v85_backupwifi.ino` | **Main NodeMCU** | RFID reading, OLED, buzzer, WiFi (+ backup network), watchdog, QR/sysinfo, internet check |
| `esp_display_v6_sysinfo.ino` | **Secondary NodeMCU** | 16x2 LCD, button, error LED |
| `worker.js` | **Cloudflare Worker** (paste into Cloudflare dashboard, not a board) | Proxies requests to Google Apps Script |

**Before uploading, install these libraries** (Arduino IDE → Sketch → Include Library → Manage Libraries):
- `MFRC522`
- `ArduinoJson`
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `QRCode` (by Richard Moore)
- `LiquidCrystal` (usually pre-installed by default)

**Before uploading the main board firmware**, fill in near the top of the file:
- `WIFI_SSID` / `WIFI_PASSWORD` — your primary network
- `WIFI_SSID2` / `WIFI_PASSWORD2` — optional backup network (leave blank to skip)

**Before deploying the Worker**, set `GSCRIPT_ID` in Cloudflare's dashboard (Settings → Variables) to your Apps Script deployment ID.

---

## 2. Wiring — Main NodeMCU

### RFID Reader (MFRC522) — SPI
| MFRC522 Pin | NodeMCU Pin |
|---|---|
| SDA (SS) | D4 |
| SCK | D5 |
| MOSI | D7 |
| MISO | D6 |
| RST | D3 |
| GND | GND |
| 3.3V | 3.3V |

### OLED (SSD1306, 0.96", SPI) — shares the SPI bus with the RFID reader
| OLED Pin | NodeMCU Pin |
|---|---|
| SCK | D5 *(shared with RFID)* |
| MOSI/SDA | D7 *(shared with RFID)* |
| CS | D8 |
| DC | D2 |
| RES | D1 |
| GND | GND |
| VCC | 3.3V |

⚠️ D8 (GPIO15) must be LOW at boot — this has worked fine in testing, but if you ever get upload/boot issues, swap `OLED_CS` to D2 and `OLED_DC` to D8.

### Buzzer
| Buzzer Pin | NodeMCU Pin |
|---|---|
| Signal (+) | D0 |
| GND (–) | GND |

### Link to Secondary Display Board
| Main NodeMCU | Secondary NodeMCU |
|---|---|
| TX (D10 / labeled "TX") | RX (labeled "RX") |
| RX (D9 / labeled "RX") | TX (labeled "TX") |
| GND | GND *(direct, dedicated wire — do not skip)* |

No voltage divider needed — both boards are 3.3V logic. Note: both boards use their single hardware UART for this link, which is the same one used for USB debug output — so once wired together, the USB Serial Monitor on either board won't show clean text while connected to the other board. Disconnect the link temporarily if you need to debug via Serial Monitor.

### Power stability
- **470µF electrolytic capacitor** across the 3.3V and GND pins, as close to the board as possible (smooths WiFi current spikes)
- Power the main board from a proper 5V/1A+ USB adapter (via micro-USB), not from a laptop USB port shared with other high-draw devices

---

## 3. Wiring — Secondary NodeMCU (Display Board)

### LCD (16x2 JHD162A, parallel HD44780, 4-bit mode)
| LCD Pin # | LCD Pin | NodeMCU Pin |
|---|---|---|
| 1 | VSS | GND |
| 2 | VDD | 3.3V or 5V *(check your LCD's rated voltage)* |
| 3 | V0 | Wiper of a 10kΩ potentiometer (contrast) |
| 4 | RS | D1 |
| 5 | RW | GND |
| 6 | E | D2 |
| 7–10 | D0–D3 | *not connected (4-bit mode)* |
| 11 | D4 | D3 |
| 12 | D5 | D4 |
| 13 | D6 | D5 |
| 14 | D7 | D6 |
| 15 | A (backlight +) | 3.3V/5V via ~220Ω resistor |
| 16 | K (backlight –) | GND |

**Potentiometer (contrast):** one outer pin → power, other outer pin → GND, middle (wiper) pin → LCD V0.

### Button (system info + QR)
| Button | NodeMCU Pin |
|---|---|
| One leg | D7 |
| Other leg | GND |

*(Uses internal pull-up in firmware — no external resistor needed.)*

### Error LED
| LED | NodeMCU Pin |
|---|---|
| Anode (+) | D8, through a ~220Ω resistor |
| Cathode (–) | GND |

### Link to Main Board
This board's **RX** pin ← Main board's **TX (D10)**
This board's **TX** pin → Main board's **RX (D9)**
This board's **GND** ↔ Main board's **GND** *(direct, dedicated wire)*

(Same link described in Section 2 — both boards use their single hardware UART pins, labeled RX/TX directly on the board.)

### Power stability
- **470µF capacitor** near this board's power input
- A smaller capacitor (~100µF) near the LCD's backlight power line helps too
- Power this board separately from the main board (a second USB adapter or the same one via a proper hub) — sharing one weak USB port between both boards was a real cause of intermittent resets during testing

---

## 4. Quick Sanity Checklist Before First Power-On

- [ ] RFID reader's SS/RST pins match the firmware's `SS_PIN`/`RST_PIN` defines
- [ ] OLED CS/DC/RES pins match the firmware's defines
- [ ] Main board and secondary board share a **direct GND wire** (not just through USB)
- [ ] `WIFI_SSID`/`WIFI_PASSWORD` filled in correctly in the main firmware
- [ ] `GSCRIPT_ID` set in the Cloudflare Worker's environment variables
- [ ] LCD contrast pot adjusted if the screen looks blank or all-black boxes
- [ ] Both boards powered from adapters capable of at least 1A each
