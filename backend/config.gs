/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM
 * Beta 3.0
 * Config.gs
 * ==========================================
 */

// ---------- Sheet Names ----------
const STAFF_SHEET = "Staff";
const SETTINGS_SHEET = "Settings";
const ATTENDANCE_PREFIX = "Attendance_";

// ---------- Default Time Zone ----------
const DEFAULT_TIMEZONE = "Asia/Kolkata";

// ---------- Cache Time ----------
const CACHE_TIME = 300;

// ---------- Read Setting ----------
function getSetting(key) {

  const sheet = SpreadsheetApp
      .getActiveSpreadsheet()
      .getSheetByName(SETTINGS_SHEET);

  if (!sheet)
    throw new Error("Settings sheet not found.");

  const data = sheet.getDataRange().getValues();

  for (let i = 1; i < data.length; i++) {

    if (String(data[i][0]).trim() === key)
      return data[i][1];

  }

  return "";
}

// ---------- API Key ----------
function getApiKey() {

  return String(getSetting("API Key"));

}

// ---------- Time Zone ----------
function getTimeZone() {

  const tz = getSetting("Time Zone");

  if (tz == "")
    return DEFAULT_TIMEZONE;

  return tz;

}

// ---------- Duplicate Delay ----------
function getDuplicateDelay() {

  const value = Number(getSetting("Duplicate Scan Delay"));

  if (isNaN(value))
    return 3;

  return value;

}

// ---------- School Name ----------
function getSchoolName() {

  return String(getSetting("School Name"));

}

// ---------- Version ----------
function getVersion() {

  return String(getSetting("Version"));

}
