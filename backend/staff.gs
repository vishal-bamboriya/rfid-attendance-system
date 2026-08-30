/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM
 * Beta 3.0
 * Staff.gs
 * ==========================================
 */


/*--------------------------------------------------
  Get Staff Sheet
---------------------------------------------------*/
function getStaffSheet() {

  const sheet = SpreadsheetApp
    .getActiveSpreadsheet()
    .getSheetByName(STAFF_SHEET);

  if (!sheet)
    throw new Error("Staff sheet not found.");

  return sheet;

}


/*--------------------------------------------------
  Find Staff By UID
---------------------------------------------------*/
function getStaffByUID(uid) {

  uid = String(uid).trim().toUpperCase();

  const sheet = getStaffSheet();

  const data = sheet.getDataRange().getValues();

  for (let i = 1; i < data.length; i++) {

    if (String(data[i][0]).trim().toUpperCase() === uid) {

      return {

        row: i + 1,

        uid: data[i][0],

        staffId: data[i][1],

        name: data[i][2],

        department: data[i][3],

        designation: data[i][4],

        status: data[i][5]

      };

    }

  }

  return null;

}


/*--------------------------------------------------
  UID Exists?
---------------------------------------------------*/
function isValidUID(uid) {

  return getStaffByUID(uid) !== null;

}


/*--------------------------------------------------
  Get Staff Name
---------------------------------------------------*/
function getStaffName(uid) {

  const staff = getStaffByUID(uid);

  if (!staff)
    return "";

  return staff.name;

}


/*--------------------------------------------------
  Get Staff ID
---------------------------------------------------*/
function getStaffID(uid) {

  const staff = getStaffByUID(uid);

  if (!staff)
    return "";

  return staff.staffId;

}


/*--------------------------------------------------
  Get Staff Status
---------------------------------------------------*/
function getStaffStatus(uid) {

  const staff = getStaffByUID(uid);

  if (!staff)
    return "";

  return staff.status;

}


/*--------------------------------------------------
  Get Department
---------------------------------------------------*/
function getDepartment(uid) {

  const staff = getStaffByUID(uid);

  if (!staff)
    return "";

  return staff.department;

}


/*--------------------------------------------------
  Get Designation
---------------------------------------------------*/
function getDesignation(uid) {

  const staff = getStaffByUID(uid);

  if (!staff)
    return "";

  return staff.designation;

}
