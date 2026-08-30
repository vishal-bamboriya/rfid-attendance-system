/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM
 * Beta 3.0
 * TestFlow.gs
 * Manual test helper — run these from the
 * Apps Script editor (select function, click Run)
 * ==========================================
 */

// Replace with a real UID that exists in your Staff sheet
const TEST_UID = "04A3B91C";

/*--------------------------------------------------
  Test 1: First scan of the day -> should Mark IN
---------------------------------------------------*/
function test1_MarkIn() {
  const result = processAttendance(TEST_UID);
  Logger.log("TEST 1 RESULT: " + JSON.stringify(result));

  if (result.type === "IN" && result.success === true) {
    Logger.log("✅ PASS: IN marked correctly");
  } else {
    Logger.log("❌ FAIL: expected type=IN, success=true");
  }
}

/*--------------------------------------------------
  Test 2: Second scan (same UID, same day) -> Mark OUT
  Run this AFTER test1_MarkIn, ideally a minute or two later
  so Working Hours isn't 00:00:00
---------------------------------------------------*/
function test2_MarkOut() {
  const result = processAttendance(TEST_UID);
  Logger.log("TEST 2 RESULT: " + JSON.stringify(result));

  if (result.type !== "OUT" || result.success !== true) {
    Logger.log("❌ FAIL: expected type=OUT, success=true");
    return;
  }

  if (/^\d{2}:\d{2}:\d{2}$/.test(result.hours) && !result.hours.includes("NaN")) {
    Logger.log("✅ PASS: Working Hours = " + result.hours);
  } else {
    Logger.log("❌ FAIL: Working Hours malformed -> " + result.hours);
  }
}

/*--------------------------------------------------
  Test 3: Third scan (same UID, same day) -> Reject
---------------------------------------------------*/
function test3_AlreadyOut() {
  const result = processAttendance(TEST_UID);
  Logger.log("TEST 3 RESULT: " + JSON.stringify(result));

  if (result.type === "ALREADY_OUT" && result.success === false) {
    Logger.log("✅ PASS: correctly rejected 3rd scan");
  } else {
    Logger.log("❌ FAIL: expected type=ALREADY_OUT, success=false");
  }
}

/*--------------------------------------------------
  Test 4: Invalid / unregistered card
---------------------------------------------------*/
function test4_InvalidCard() {
  const result = processAttendance("FFFFFFFF");
  Logger.log("TEST 4 RESULT: " + JSON.stringify(result));

  if (result.type === "INVALID" && result.success === false) {
    Logger.log("✅ PASS: correctly rejected invalid UID");
  } else {
    Logger.log("❌ FAIL: expected type=INVALID, success=false");
  }
}

/*--------------------------------------------------
  Test 5: Directly re-read today's record and inspect
  the raw types coming back from the sheet
  (confirms the Date-object bug is actually gone)
---------------------------------------------------*/
function test5_InspectStoredTypes() {
  const staff = getStaffByUID(TEST_UID);
  if (!staff) {
    Logger.log("❌ Staff not found, check TEST_UID");
    return;
  }

  const record = getTodayAttendanceRecord(staff.staffId);
  if (!record) {
    Logger.log("No record yet today — run test1_MarkIn first");
    return;
  }

  Logger.log("inTime value      : " + record.inTime);
  Logger.log("inTime typeof     : " + typeof record.inTime);
  Logger.log("outTime value     : " + record.outTime);
  Logger.log("workingHours value: " + record.workingHours);

  if (typeof record.inTime === "string" && /^\d{2}:\d{2}:\d{2}$/.test(record.inTime)) {
    Logger.log("✅ PASS: inTime is a clean HH:mm:ss string");
  } else {
    Logger.log("❌ FAIL: inTime is not a clean string -> check column formatting");
  }
}
