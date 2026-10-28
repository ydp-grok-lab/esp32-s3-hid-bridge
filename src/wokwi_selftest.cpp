#include "wokwi_selftest.h"

#ifdef WOKWI_SELFTEST

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"
#include "wifi_setup.h"

// In-simulator end-to-end test of the real HTTP stack (WebServer + lwIP + ArduinoJson +
// hidExecOp with the HID stub). The firmware connects to itself, so no host port
// forwarding is required (the wokwi-cli has no [[net.forward]] support).

static int gPass = 0;
static int gFail = 0;

static void report(const char* name, bool ok, const String& detail) {
  if (ok) {
    gPass++;
    Serial.printf("[selftest] PASS %s\n", name);
  } else {
    gFail++;
    Serial.printf("[selftest] FAIL %s :: %s\n", name, detail.c_str());
  }
}

static String gBase;

static int doRequest(const char* method, const char* path, const String& body, String& resp) {
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(8000);
  if (!http.begin(gBase + path)) return -100;
  http.addHeader("Content-Type", "application/json");
  int code;
  if (strcmp(method, "POST") == 0) code = http.POST(body);
  else if (strcmp(method, "GET") == 0) code = http.GET();
  else code = http.sendRequest(method, body);
  resp = (code > 0) ? http.getString() : String();
  http.end();
  return code;
}

static void expectPost(const char* name, const String& body, int wantCode, const char* wantSub) {
  String resp;
  int code = doRequest("POST", "/v1/action", body, resp);
  bool ok = (code == wantCode) && (wantSub == nullptr || resp.indexOf(wantSub) >= 0);
  report(name, ok, String("http=") + code + " body=" + resp);
}

static void runAll() {
  String resp;
  int code;

  // --- /v1/health
  code = doRequest("GET", "/v1/health", "", resp);
  report("health.status", code == 200, String("http=") + code);
  report("health.ok", resp.indexOf("\"ok\":true") >= 0, resp);
  report("health.device", resp.indexOf("\"device\":\"" DEVICE_NAME "\"") >= 0, resp);
  report("health.mode", resp.indexOf("\"mode\":\"" ) >= 0 && resp.indexOf(gWifiMode) >= 0, resp);
  report("health.screen", resp.indexOf("\"screen_width\":" ) >= 0 && resp.indexOf("\"screen_height\":") >= 0, resp);

  // --- happy path: every op once
  expectPost("action.all_ops",
             "{\"ops\":["
             "{\"op\":\"click\",\"x\":100,\"y\":200},"
             "{\"op\":\"move\",\"x\":10,\"y\":-5,\"relative\":true},"
             "{\"op\":\"move\",\"x\":300,\"y\":400},"
             "{\"op\":\"drag\",\"x1\":10,\"y1\":10,\"x2\":200,\"y2\":200},"
             "{\"op\":\"scroll\",\"dy\":-3},"
             "{\"op\":\"scroll\",\"dy\":2,\"dx\":1},"
             "{\"op\":\"type\",\"text\":\"hello\"},"
             "{\"op\":\"key\",\"key\":\"enter\"},"
             "{\"op\":\"key\",\"key\":\"ctrl+c\"},"
             "{\"op\":\"key\",\"key\":\"cmd+space\"},"
             "{\"op\":\"wait\",\"ms\":10}]}",
             200, "{\"ok\":true}");

  // --- timing: wait must really block for ~ms
  {
    uint32_t t0 = millis();
    expectPost("action.wait_200ms", "{\"ops\":[{\"op\":\"wait\",\"ms\":200}]}", 200, "\"ok\":true");
    uint32_t dt = millis() - t0;
    report("action.wait_elapsed>=200", dt >= 200 && dt < 3000, String("dt=") + dt);
  }

  // --- error paths
  expectPost("err.invalid_json", "{not json", 400, "invalid json");
  expectPost("err.empty_body", "", 400, "invalid json");
  expectPost("err.missing_ops", "{\"foo\":1}", 400, "missing ops array");
  expectPost("err.ops_not_array", "{\"ops\":5}", 400, "missing ops array");
  expectPost("err.unknown_op", "{\"ops\":[{\"op\":\"teleport\"}]}", 400, "unknown op: teleport");
  expectPost("err.missing_op_name", "{\"ops\":[{\"x\":1}]}", 400, "unknown op");
  expectPost("err.unknown_key", "{\"ops\":[{\"op\":\"key\",\"key\":\"nonsense\"}]}", 400, "unknown key: nonsense");
  expectPost("err.bad_chord", "{\"ops\":[{\"op\":\"key\",\"key\":\"ctrl+nonsense\"}]}", 400, "unknown key");
  {
    String big = "{\"ops\":[";
    for (int i = 0; i <= MAX_OPS_PER_REQUEST; i++) {
      if (i) big += ",";
      big += "{\"op\":\"scroll\",\"dy\":0}";
    }
    big += "]}";
    expectPost("err.too_many_ops", big, 400, "too many ops");
  }
  {
    String longText = "{\"ops\":[{\"op\":\"type\",\"text\":\"";
    for (int i = 0; i < MAX_TYPE_CHARS + 1; i++) longText += 'a';
    longText += "\"}]}";
    expectPost("err.text_too_long", longText, 400, "text too long");
  }
  // an error stops execution but earlier ops were already executed (documented behaviour)
  expectPost("err.partial_then_error",
             "{\"ops\":[{\"op\":\"click\",\"x\":1,\"y\":1},{\"op\":\"bogus\"}]}", 400, "unknown op: bogus");

  // --- routing
  code = doRequest("GET", "/nope", "", resp);
  report("route.404", code == 404 && resp.indexOf("not found") >= 0, String("http=") + code + " " + resp);
  code = doRequest("OPTIONS", "/v1/action", "", resp);
  report("route.options_204", code == 204, String("http=") + code);
  code = doRequest("GET", "/index.html", "", resp);
  report("route.index_html", code == 200 && resp.indexOf("ESP32 HID Bridge") >= 0, String("http=") + code);

  Serial.printf("[selftest] RESULT pass=%d fail=%d\n", gPass, gFail);
  if (gFail == 0) Serial.println("[selftest] ALL PASSED");
  else Serial.println("[selftest] SOME FAILED");
}

static void selftestTask(void*) {
  // Wait for the network (STA via Wokwi-GUEST, or AP in non-STA configs).
  vTaskDelay(pdMS_TO_TICKS(500));
  Serial.printf("[selftest] start mode=%s ip=%s\n", gWifiMode.c_str(), gWifiIp.c_str());

  // Prefer loopback; fall back to the device's own IP if lwIP loopback is unavailable.
  gBase = "http://127.0.0.1:" + String(HTTP_PORT);
  String resp;
  if (doRequest("GET", "/v1/health", "", resp) < 0) {
    Serial.println("[selftest] loopback 127.0.0.1 failed, trying own IP");
    gBase = "http://" + gWifiIp + ":" + String(HTTP_PORT);
  }

  runAll();
  vTaskDelete(nullptr);
}

void wokwiSelftestBegin() {
  xTaskCreatePinnedToCore(selftestTask, "selftest", 10240, nullptr, 1, nullptr, 0);
}

#endif  // WOKWI_SELFTEST
