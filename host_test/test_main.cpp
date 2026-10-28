// Host-side tests for the JSON/action layer of the HID bridge.
// Compiles the REAL src/action_handler.cpp + src/hid_actions.cpp against the
// Arduino shim (host_test/shim) and the HID stub (include/hid_stub.h).
// Assertions look at (a) the HTTP status + JSON body that /v1/action would
// return and (b) the "[hid-stub] ..." trace of HID events that would be sent.
#include <Arduino.h>
#include <ArduinoJson.h>

#include "action_handler.h"
#include "config.h"
#include "hid_actions.h"

// ---- shim state / globals normally provided by the Arduino core + wifi_setup.cpp ----
uint32_t g_millis = 0;
std::string g_serial;
HostSerial Serial;
String gWifiMode = "sta";
String gWifiIp = "10.13.37.2";

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (cond) { g_pass++; }                                                     \
    else { g_fail++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); }   \
  } while (0)

static bool has(const char* needle) { return g_serial.find(needle) != std::string::npos; }
static void resetTrace() { g_serial.clear(); }

struct Result { int code; std::string body; };
static Result post(const char* json) {
  String out;
  int code = actionHandleBody(String(json), out);
  return {code, out.c_str()};
}
static bool bodyHas(const Result& r, const char* s) { return r.body.find(s) != std::string::npos; }

static void test_health() {
  puts("health");
  String h = actionBuildHealthJson();
  JsonDocument d;
  CHECK(!deserializeJson(d, h), "health is valid json");
  CHECK(d["ok"] == true, "ok true");
  CHECK(String(d["device"] | "") == DEVICE_NAME, "device name");
  CHECK(String(d["mode"] | "") == "sta", "mode");
  CHECK(String(d["ip"] | "") == "10.13.37.2", "ip");
  CHECK((int)d["screen_width"] == SCREEN_WIDTH && (int)d["screen_height"] == SCREEN_HEIGHT, "screen size");
  CHECK(d["uptime_ms"].is<uint32_t>(), "uptime");
}

static void test_invalid_requests() {
  puts("invalid requests");
  Result r;
  r = post("{not json");        CHECK(r.code == 400 && bodyHas(r, "invalid json"), "bad json");
  r = post("");                 CHECK(r.code == 400 && bodyHas(r, "invalid json"), "empty body");
  r = post("{}");               CHECK(r.code == 400 && bodyHas(r, "missing ops array"), "no ops");
  r = post("{\"ops\":5}");      CHECK(r.code == 400 && bodyHas(r, "missing ops array"), "ops not array");
  r = post("{\"ops\":{}}");     CHECK(r.code == 400 && bodyHas(r, "missing ops array"), "ops object");
  r = post("{\"ops\":[{\"op\":\"teleport\"}]}");
  CHECK(r.code == 400 && bodyHas(r, "unknown op: teleport") && bodyHas(r, "\"ok\":false"), "unknown op");
  r = post("{\"ops\":[{\"x\":1}]}");  CHECK(r.code == 400 && bodyHas(r, "unknown op"), "missing op name");
  r = post("{\"ops\":[{\"op\":\"key\",\"key\":\"nonsense\"}]}");
  CHECK(r.code == 400 && bodyHas(r, "unknown key: nonsense"), "unknown key");
  r = post("{\"ops\":[{\"op\":\"key\",\"key\":\"ctrl+nonsense\"}]}");
  CHECK(r.code == 400 && bodyHas(r, "unknown key"), "bad chord part");
  r = post("{\"ops\":[{\"op\":\"key\"}]}");
  CHECK(r.code == 400 && bodyHas(r, "unknown key"), "missing key field");
}

static void test_limits() {
  puts("limits");
  std::string big = "{\"ops\":[";
  for (int i = 0; i <= MAX_OPS_PER_REQUEST; i++) { if (i) big += ","; big += "{\"op\":\"scroll\",\"dy\":0}"; }
  big += "]}";
  Result r = post(big.c_str());
  CHECK(r.code == 400 && bodyHas(r, "too many ops"), "MAX_OPS+1 rejected");

  std::string ok = "{\"ops\":[";
  for (int i = 0; i < MAX_OPS_PER_REQUEST; i++) { if (i) ok += ","; ok += "{\"op\":\"scroll\",\"dy\":0}"; }
  ok += "]}";
  r = post(ok.c_str());
  CHECK(r.code == 200, "exactly MAX_OPS accepted");

  std::string t = "{\"ops\":[{\"op\":\"type\",\"text\":\"" + std::string(MAX_TYPE_CHARS + 1, 'a') + "\"}]}";
  r = post(t.c_str());
  CHECK(r.code == 400 && bodyHas(r, "text too long"), "text too long");
  t = "{\"ops\":[{\"op\":\"type\",\"text\":\"" + std::string(MAX_TYPE_CHARS, 'a') + "\"}]}";
  r = post(t.c_str());
  CHECK(r.code == 200, "text of exactly MAX_TYPE_CHARS accepted");
}

static void test_mouse_ops() {
  puts("mouse ops");
  Result r;
  // click at screen centre -> HID abs ~16383; press/release button 1
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"click\",\"x\":960,\"y\":540}]}");
  CHECK(r.code == 200 && r.body == "{\"ok\":true}", "click ok");
  CHECK(has("mouse.move x=16383 y=16383"), "click maps 960,540 -> 16383,16383");
  CHECK(has("mouse.press b=1") && has("mouse.release b=1"), "click press+release");
  CHECK(g_serial.find("mouse.press") < g_serial.find("mouse.release"), "press before release");

  // clamping of out-of-range coordinates
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"move\",\"x\":99999,\"y\":-50}]}");
  CHECK(r.code == 200, "move ok");
  CHECK(has("mouse.move x=32767 y=0"), "absolute move clamps to screen");

  // relative move is relative to the last absolute position (1920,0 after clamp)
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"move\",\"x\":-960,\"y\":540,\"relative\":true}]}");
  CHECK(r.code == 200 && has("mouse.move x=16383 y=16383"), "relative move from last position");

  // drag: starts at (x1,y1), 24 interpolated steps, ends at (x2,y2), press->release
  resetTrace();
  uint32_t t0 = millis();
  r = post("{\"ops\":[{\"op\":\"drag\",\"x1\":0,\"y1\":0,\"x2\":1920,\"y2\":1080}]}");
  CHECK(r.code == 200, "drag ok");
  size_t moves = 0;
  for (size_t p = g_serial.find("mouse.move"); p != std::string::npos; p = g_serial.find("mouse.move", p + 1)) moves++;
  CHECK(moves == 25, "drag = 1 start move + 24 steps");
  CHECK(has("mouse.move x=32767 y=32767"), "drag ends at bottom-right");
  CHECK(millis() - t0 >= 15 + CLICK_PRESS_MS + 24 * DRAG_STEP_MS, "drag consumed expected time");

  // scroll: wheel/pan sent on the absolute report, values clamped to int8
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"scroll\",\"dy\":-3,\"dx\":2},{\"op\":\"scroll\",\"dy\":999}]}");
  CHECK(r.code == 200 && has("wheel=-3 pan=2") && has("wheel=127 pan=0"), "scroll values + clamp");
}

static void test_keyboard_ops() {
  puts("keyboard ops");
  Result r;
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"type\",\"text\":\"Hi!\"}]}");
  CHECK(r.code == 200 && has("key.write 0x48") && has("key.write 0x69") && has("key.write 0x21"), "type ascii");

  resetTrace();
  r = post("{\"ops\":[{\"op\":\"type\",\"text\":\"a\\u00e9\\u4e2db\"}]}");  // non-ASCII is skipped
  CHECK(r.code == 200 && has("key.write 0x61") && has("key.write 0x62"), "type skips non-ascii, keeps ascii");
  size_t writes = 0;
  for (size_t p = g_serial.find("key.write"); p != std::string::npos; p = g_serial.find("key.write", p + 1)) writes++;
  CHECK(writes == 2, "only 2 ascii chars written");

  resetTrace();
  r = post("{\"ops\":[{\"op\":\"key\",\"key\":\"Enter\"}]}");
  CHECK(r.code == 200 && has("key.press 0xB0") && has("key.release 0xB0"), "named key (case-insensitive)");

  resetTrace();
  r = post("{\"ops\":[{\"op\":\"key\",\"key\":\"ctrl+shift+t\"}]}");
  CHECK(r.code == 200, "chord ok");
  size_t pc = g_serial.find("key.press 0x80"), ps = g_serial.find("key.press 0x81"), pt = g_serial.find("key.press 0x74");
  size_t rt = g_serial.find("key.release 0x74"), rs = g_serial.find("key.release 0x81"), rc = g_serial.find("key.release 0x80");
  CHECK(pc < ps && ps < pt && pt < rt && rt < rs && rs < rc, "chord press order mods->key, release reverse");

  resetTrace();
  r = post("{\"ops\":[{\"op\":\"key\",\"key\":\"cmd+space\"}]}");
  CHECK(r.code == 200 && has("key.press 0x83") && has("key.press 0x20"), "cmd+space");

  resetTrace();
  r = post("{\"ops\":[{\"op\":\"key\",\"key\":\"a\"}]}");
  CHECK(r.code == 200 && has("key.write 0x61"), "single char key");
}

static void test_wait_and_sequencing() {
  puts("wait / sequencing");
  uint32_t t0 = millis();
  Result r = post("{\"ops\":[{\"op\":\"wait\",\"ms\":250}]}");
  CHECK(r.code == 200 && millis() - t0 == 250, "wait 250ms");

  t0 = millis();
  r = post("{\"ops\":[{\"op\":\"wait\",\"ms\":999999}]}");
  CHECK(r.code == 200 && millis() - t0 == 60000, "wait clamped to 60s");

  // ops run in order; an error stops the batch, earlier ops already happened
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"click\",\"x\":1,\"y\":1},{\"op\":\"bogus\"},{\"op\":\"key\",\"key\":\"enter\"}]}");
  CHECK(r.code == 400 && bodyHas(r, "unknown op: bogus"), "error reported");
  CHECK(has("mouse.press") && !has("key.press 0xB0"), "ops before error ran, ops after did not");

  // op names are case/whitespace tolerant
  resetTrace();
  r = post("{\"ops\":[{\"op\":\"  CLICK \",\"x\":0,\"y\":0}]}");
  CHECK(r.code == 200 && has("mouse.press"), "op name trimmed + lowercased");
}

int main() {
  hidBegin();  // required: hidExecOp is a no-op error path until HID is "ready"
  test_health();
  test_invalid_requests();
  test_limits();
  test_mouse_ops();
  test_keyboard_ops();
  test_wait_and_sequencing();
  printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
