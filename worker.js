/**
 * RFID Attendance System — Cloudflare Worker Proxy
 *
 * PURPOSE:
 * The NodeMCU (ESP8266) used to call Google Apps Script's /exec URL directly.
 * That endpoint ALWAYS issues an HTTP 302 redirect (script.google.com ->
 * script.googleusercontent.com) to fetch the real response. Because the
 * ESP8266 has no hardware crypto, doing TWO full TLS handshakes took 10-30s.
 *
 * This Worker sits in between:
 *   NodeMCU --(1 TLS handshake, to Cloudflare's edge)--> Worker
 *   Worker  --(follows Google's redirect internally, on fast infra)--> Apps Script
 *   Worker  --(clean single JSON response)--> NodeMCU
 *
 * The NodeMCU now only ever talks to ONE host (this Worker's URL) and never
 * sees Google's redirect at all.
 *
 * SETUP:
 * 1. Deploy this file as a Cloudflare Worker.
 * 2. In the Worker's dashboard -> Settings -> Variables, add a variable:
 *      Name:  GSCRIPT_ID
 *      Value: <the ID from your Apps Script /exec URL>
 *              (the part between /macros/s/ and /exec)
 *    Mark it as "Encrypt" if you want it hidden in the dashboard (optional,
 *    but recommended since it's effectively a secret path).
 * 3. Update the NodeMCU firmware to call this Worker's URL instead of
 *    script.google.com directly (see firmware notes below).
 */

export default {
  async fetch(request, env) {
    // Only allow GET, matching the existing Apps Script doGet() contract
    if (request.method !== "GET") {
      return jsonResponse({ status: "ERROR", message: "Method not allowed" }, 405);
    }

    // GSCRIPT_ID must be set in the Worker's environment variables.
    const gscriptId = env.GSCRIPT_ID;
    if (!gscriptId) {
      return jsonResponse(
        { status: "ERROR", message: "GSCRIPT_ID not configured in Worker env vars" },
        500
      );
    }

    // Pass through all incoming query params (uid, key, etc.) unchanged
    const incomingUrl = new URL(request.url);
    const targetUrl = `https://script.google.com/macros/s/${gscriptId}/exec${incomingUrl.search}`;

    try {
      // fetch() on Cloudflare's edge follows the 302 redirect to
      // script.googleusercontent.com automatically and fast (Cloudflare's
      // network + real crypto hardware), then returns the final response.
      const gasResponse = await fetch(targetUrl, {
        method: "GET",
        redirect: "follow",
        cf: {
          // Don't cache attendance calls — every scan must hit Apps Script live
          cacheTtl: 0,
          cacheEverything: false,
        },
      });

      const bodyText = await gasResponse.text();

      // Apps Script should already be returning JSON (per Attendance.gs).
      // We just relay it straight through with the correct content-type,
      // so the ESP8266's existing ArduinoJson parsing code doesn't need to change.
      return new Response(bodyText, {
        status: 200,
        headers: {
          "Content-Type": "application/json",
          // Helpful for debugging in a browser; harmless for the ESP8266
          "Access-Control-Allow-Origin": "*",
        },
      });
    } catch (err) {
      return jsonResponse(
        { status: "ERROR", message: "Proxy fetch failed: " + err.message },
        502
      );
    }
  },
};

function jsonResponse(obj, status) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}
