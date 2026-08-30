/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM
 * Beta 3.0
 * Attendance.gs
 * ==========================================
 */


/*--------------------------------------------------
  Find Today's Attendance Record
---------------------------------------------------*/
function getTodayAttendanceRecord(staffId) {

  const sheet = getAttendanceSheet();
  const data = sheet.getDataRange().getValues();

  const today = getCurrentDate();

  for (let i = 1; i < data.length; i++) {

    const cellValue = data[i][0];

const sheetDate = Utilities.formatDate(
  new Date(cellValue),
  getTimeZone(),
  "yyyy-MM-dd"
);

    const rowStaffId = String(data[i][2]).trim();

    Logger.log("Today      : " + today);
Logger.log("Sheet Date : " + sheetDate);
Logger.log("StaffID    : " + rowStaffId);
Logger.log("Match      : " + (sheetDate === today && rowStaffId === String(staffId).trim()));
    

    
    if (sheetDate === today && rowStaffId === String(staffId).trim()) {

      return {

        row: i + 1,

        date: sheetDate,

        day: data[i][1],

        staffId: rowStaffId,

        name: data[i][3],

       inTime: formatTimeCell(data[i][4]),

        outTime: formatTimeCell(data[i][5]),

        workingHours: data[i][6] ? String(data[i][6]).trim() : "",

      };

    }

  }

  return null;

}
/*--------------------------------------------------
  Mark IN
---------------------------------------------------*/
function markIn(staff) {

  const sheet = getAttendanceSheet();

  sheet.appendRow([

    getCurrentDate(),

    getCurrentDay(),

    staff.staffId,

    staff.name,

    getCurrentTime(),

    "",

    ""

  ]);

  return {

    success: true,

    type: "IN",

    staffId: staff.staffId,

    name: staff.name,

    time: getCurrentTime()

  };

}
/*--------------------------------------------------
  Mark OUT
---------------------------------------------------*/
function markOut(record) {

  const sheet = getAttendanceSheet();

  const outTime = getCurrentTime();
  Logger.log("IN RAW = " + JSON.stringify(record.inTime));
Logger.log("OUT RAW = " + JSON.stringify(outTime));
Logger.log("IN LENGTH = " + String(record.inTime).length);
Logger.log("OUT LENGTH = " + String(outTime).length);
  const workingHours = calculateWorkingHours(
    record.inTime,
    outTime
  );

  // Update OUT Time
  sheet.getRange(record.row, 6).setValue(outTime);

  // Update Working Hours
  sheet.getRange(record.row, 7).setValue(workingHours);

  return {

    success: true,

    type: "OUT",

    staffId: record.staffId,

    name: record.name,

    time: outTime,

    hours: workingHours

  };

}
/*--------------------------------------------------
  Process Attendance
---------------------------------------------------*/
function processAttendance(uid) {

  // Find Staff
  const staff = getStaffByUID(uid);

  if (staff == null) {

    return {

      success: false,

      type: "INVALID",

      message: "Card Not Registered"

    };

  }

  // Check Active Status
  if (String(staff.status).toUpperCase() != "ACTIVE") {

    return {

      success: false,

      type: "INACTIVE",

      message: "Staff Inactive"

    };

  }

  // Find Today's Record
  const record = getTodayAttendanceRecord(staff.staffId);

  // No Record -> Mark IN
  if (record == null) {

    return markIn(staff);

  }

  // Record Found
  if (record.outTime == "" || record.outTime == null) {

    return markOut(record);

  }

  // Already OUT

  return {

    success: false,

    type: "ALREADY_OUT",

    name: staff.name,

    message: "Attendance Already Completed"

  };

}
