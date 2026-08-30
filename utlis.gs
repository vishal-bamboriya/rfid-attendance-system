/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM
 * Beta 3.0
 * Utils.gs
 * ==========================================
 */


/*--------------------------------------------------
  Return JSON Response
---------------------------------------------------*/
function sendResponse(obj) {

  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);

}


/*--------------------------------------------------
  Current Date (YYYY-MM-DD)
---------------------------------------------------*/
function getCurrentDate() {

  return Utilities.formatDate(
    new Date(),
    getTimeZone(),
    "yyyy-MM-dd"
  );

}


/*--------------------------------------------------
  Current Time (HH:mm:ss)
---------------------------------------------------*/
function getCurrentTime() {

  return Utilities.formatDate(
    new Date(),
    getTimeZone(),
    "HH:mm:ss"
  );

}


/*--------------------------------------------------
  Current Day
---------------------------------------------------*/
function getCurrentDay() {

  return Utilities.formatDate(
    new Date(),
    getTimeZone(),
    "EEEE"
  );

}


/*--------------------------------------------------
  Current Month Sheet Name
---------------------------------------------------*/
function getAttendanceSheetName() {

  return ATTENDANCE_PREFIX +

    Utilities.formatDate(
      new Date(),
      getTimeZone(),
      "MMMM_yyyy"
    );

}


/*--------------------------------------------------
  Get Attendance Sheet
---------------------------------------------------*/
function getAttendanceSheet() {

  const ss = SpreadsheetApp.getActiveSpreadsheet();

  const sheetName = getAttendanceSheetName();

  let sheet = ss.getSheetByName(sheetName);

  if (!sheet) {

    sheet = createAttendanceSheet(sheetName);

  }

  return sheet;

}


/*--------------------------------------------------
  Create Monthly Attendance Sheet
---------------------------------------------------*/
function createAttendanceSheet(sheetName) {

  const ss = SpreadsheetApp.getActiveSpreadsheet();

  const sheet = ss.insertSheet(sheetName);

  sheet.appendRow([
    "Date",
    "Day",
    "Staff ID",
    "Name",
    "IN Time",
    "OUT Time",
    "Working Hours"
  ]);

  sheet.getRange(1,1,1,7)
       .setFontWeight("bold");

  // Force IN Time / OUT Time columns to plain text so Sheets
  // never auto-converts "08:00:00" strings into Date/Time values
  sheet.getRange(2, 5, sheet.getMaxRows() - 1, 2)
       .setNumberFormat("@");

  sheet.setFrozenRows(1);

  return sheet;

}


/*--------------------------------------------------
/*--------------------------------------------------
  Calculate Working Hours
---------------------------------------------------*/
function calculateWorkingHours(inTime, outTime) {
  Logger.log("calculateWorkingHours()");
Logger.log("IN = " + JSON.stringify(inTime));
Logger.log("OUT = " + JSON.stringify(outTime));

  inTime = String(inTime).trim();
  outTime = String(outTime).trim();

  const inParts = inTime.split(":");
  const outParts = outTime.split(":");

  if (inParts.length != 3 || outParts.length != 3) {
    return "00:00:00";
  }

  const inSeconds =
      Number(inParts[0]) * 3600 +
      Number(inParts[1]) * 60 +
      Number(inParts[2]);

  const outSeconds =
      Number(outParts[0]) * 3600 +
      Number(outParts[1]) * 60 +
      Number(outParts[2]);

  let diff = outSeconds - inSeconds;

  if (diff < 0) diff += 24 * 3600;   // Midnight cross support

  const hours = Math.floor(diff / 3600);
  diff %= 3600;

  const minutes = Math.floor(diff / 60);
  const seconds = diff % 60;

  return pad(hours) + ":" +
         pad(minutes) + ":" +
         pad(seconds);
}

/*--------------------------------------------------
  Safely Format a Time Cell
  (Handles both plain strings AND cells that Sheets
  auto-converted into Date objects, e.g. "08:00:00"
  entered as text but read back as a Date)
---------------------------------------------------*/
function formatTimeCell(cellValue) {

  if (!cellValue) return "";

  if (cellValue instanceof Date) {
    return Utilities.formatDate(cellValue, getTimeZone(), "HH:mm:ss");
  }

  return String(cellValue).trim();
}


/*--------------------------------------------------
  Pad Zero
---------------------------------------------------*/
function pad(num) {

  return ("0" + num).slice(-2);

}


/*--------------------------------------------------
  Current Timestamp
---------------------------------------------------*/
function getTimestamp() {

  return Utilities.formatDate(

    new Date(),

    getTimeZone(),

    "yyyy-MM-dd HH:mm:ss"

  );

}
