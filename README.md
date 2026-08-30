# RFID Staff Attendance System

A smart attendance kiosk built on ESP8266 (NodeMCU) that reads RFID cards and logs staff IN/OUT times automatically to Google Sheets — with a fast cloud sync, a secondary status display, and a polished icon-based UI.

---

## 📸 Photos

Some shots from the build/debugging process:

| | |
|---|---|
| ![RFID reader module close-up](images/rfid-reader-module.jpg) | ![RFID reader + OLED wired on breadboard](images/rfid-oled-closeup.jpg) |
| *MFRC522 RFID reader module* | *RFID reader + OLED wired together, showing "Ready / Tap your card"* |
| ![Serial Monitor debugging session](images/serial-monitor-test.jpg) | ![Full workbench setup with LCD, breadboards, and laptop](images/full-workbench-setup.jpg) |
| *Debugging a scan cycle via Serial Monitor* | *Full workbench setup — LCD, RFID, OLED, and two ESP8266 boards mid-development* |

*(More photos of the final assembled kiosk can be added here later.)*

---

## 🎥 Demo Video

<!--
  Same drag-and-drop trick works for short video clips (.mp4) too —
  GitHub will embed it as a playable video right in this README.
-->

*(A short demo video goes here)*

---

## What It Does

- Tap a registered RFID card → first scan of the day marks **IN**, second scan marks **OUT** (and calculates hours worked)
- A third scan the same day is rejected as **Already Done**
- Unregistered cards are rejected as **Unknown Card**
- All data is written live to a Google Sheet, organized one tab per month
- A small OLED shows quick icon-based feedback (checkmark/cross/loading), while a 16x2 LCD shows full readable details (greeting, name, time)
- A button on the display board shows live system diagnostics as a QR code + text (WiFi status, uptime, memory)

---

## Hardware

- **Main board:** NodeMCU (ESP8266) — RFID reader, OLED, buzzer, WiFi
- **Secondary board:** a second NodeMCU (ESP8266) — 16x2 LCD, button, status LED
- **RFID reader:** MFRC522
- **Displays:** 0.96" SSD1306 OLED (SPI) + 16x2 JHD162A LCD (parallel)

Full wiring diagrams: see [`docs/wiring_and_files.md`](docs/wiring_and_files.md)

---

## Backend

- **Google Apps Script** — writes attendance to Google Sheets, one sheet per month
- **Google Sheets** — the actual data store (`Staff` sheet for registered cards, `Settings` sheet for config)
- **Cloudflare Worker** — a lightweight proxy that speeds up every scan from ~10-30 seconds down to ~3-6 seconds, by handling Google's internal redirect on faster infrastructure

---

## Getting Started

New to this project? Follow the full setup guide from scratch:

👉 **[`docs/setup_guide.md`](docs/setup_guide.md)** — step-by-step instructions (Google Sheet → Apps Script → Cloudflare Worker → hardware wiring → firmware upload → testing)

For a deeper technical overview of every component used:

👉 **[`docs/project_overview.md`](docs/project_overview.md)**

---

## Repository Structure

```
firmware/
  main-board/       -> firmware for the RFID + OLED board
  display-board/     -> firmware for the LCD + button board
backend/
  Code.gs, Config.gs, Staff.gs, Attendance.gs, Utils.gs, TestFlow.gs
  worker.js          -> Cloudflare Worker proxy code
docs/
  setup_guide.md
  wiring_and_files.md
  project_overview.md
```

---

## ⚠️ Before You Deploy

If you clone/fork this repo, you must fill in your own values (do NOT reuse the placeholders):
- WiFi SSID/password in the main board firmware
- Your own Cloudflare Worker URL
- Your own API Key (set in the Google Sheet's `Settings` tab, and matched in the firmware)
- Your own Google Apps Script deployment ID (set in the Cloudflare Worker's environment variables, not in the firmware)

Never commit real WiFi passwords or API keys to a public repository.

---

## Status

🚧 Beta 3.0 — core attendance flow, cloud sync, and dual-display UI are stable and tested. Physical enclosure and multi-kiosk support are not yet built.
