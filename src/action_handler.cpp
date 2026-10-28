#include "action_handler.h"
#include "config.h"
#include "hid_actions.h"
#include "wifi_setup.h"

#include <ArduinoJson.h>

String actionBuildHealthJson() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["device"] = DEVICE_NAME;
  doc["uptime_ms"] = (uint32_t)millis();
  doc["ip"] = gWifiIp;
  doc["mode"] = gWifiMode;
  doc["screen_width"] = SCREEN_WIDTH;
  doc["screen_height"] = SCREEN_HEIGHT;

  String out;
  serializeJson(doc, out);
  return out;
}

int actionHandleBody(const String& body, String& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    out = String("{\"ok\":false,\"error\":\"invalid json: ") + err.c_str() + "\"}";
    return 400;
  }

  JsonArray ops = doc["ops"].as<JsonArray>();
  if (ops.isNull()) {
    out = "{\"ok\":false,\"error\":\"missing ops array\"}";
    return 400;
  }
  if ((int)ops.size() > MAX_OPS_PER_REQUEST) {
    out = "{\"ok\":false,\"error\":\"too many ops\"}";
    return 400;
  }

  String errOut;
  for (JsonObject opObj : ops) {
    const char* op = opObj["op"] | "";
    int32_t x = opObj["x"] | 0;
    int32_t y = opObj["y"] | 0;
    int32_t x1 = opObj["x1"] | 0;
    int32_t y1 = opObj["y1"] | 0;
    int32_t x2 = opObj["x2"] | 0;
    int32_t y2 = opObj["y2"] | 0;
    int32_t dy = opObj["dy"] | 0;
    int32_t dx = opObj["dx"] | 0;
    bool relative = opObj["relative"] | false;
    const char* text = opObj["text"] | "";
    const char* key = opObj["key"] | "";
    uint32_t ms = opObj["ms"] | 0;

    if (!hidExecOp(String(op), x, y, x1, y1, x2, y2, dy, dx, relative,
                   String(text), String(key), ms, errOut)) {
      JsonDocument resp;
      resp["ok"] = false;
      resp["error"] = errOut;
      out = "";
      serializeJson(resp, out);
      return 400;
    }
  }

  out = "{\"ok\":true}";
  return 200;
}
