/**
 * ==========================================
 * RFID STAFF ATTENDANCE SYSTEM
 * Beta 3.0
 * Code.gs
 * ==========================================
 */

function doGet(e) {

  try {

    // Check Parameters
    if (!e || !e.parameter) {

      return sendResponse({

        success: false,

        type: "ERROR",

        message: "No Parameters"

      });

    }

    // API Key
    const apiKey = String(e.parameter.key || "").trim();

    if (apiKey !== getApiKey()) {

      return sendResponse({

        success: false,

        type: "UNAUTHORIZED",

        message: "Invalid API Key"

      });

    }

    // UID
    const uid = String(e.parameter.uid || "")
      .trim()
      .toUpperCase();

    if (uid == "") {

      return sendResponse({

        success: false,

        type: "INVALID_UID",

        message: "UID Missing"

      });

    }

    // Process Attendance
    const result = processAttendance(uid);

    return sendResponse(result);

  }

  catch (err) {

    return sendResponse({

      success: false,

      type: "SERVER_ERROR",

      message: err.toString()

    });

  }

}
