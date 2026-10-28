#include "http_api.h"
#include "config.h"
#include "action_handler.h"
#include "wifi_setup.h"

WebServer gServer(HTTP_PORT);

static void addCors() {
  gServer.sendHeader("Access-Control-Allow-Origin", "*");
  gServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  gServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleOptions() {
  addCors();
  gServer.send(204);
}

static void handleHealth() {
  addCors();
  gServer.send(200, "application/json", actionBuildHealthJson());
}

static void handleAction() {
  addCors();

  if (gServer.method() != HTTP_POST) {
    gServer.send(405, "application/json", "{\"ok\":false,\"error\":\"POST required\"}");
    return;
  }

  String body = gServer.arg("plain");
  if (body.length() == 0) {
    // Some clients send as raw body without plain; try first arg
    if (gServer.args() > 0) body = gServer.arg(0);
  }

  // JSON parsing / validation / op execution lives in action_handler.cpp
  String out;
  int code = actionHandleBody(body, out);
  gServer.send(code, "application/json", out);
}

static void handleRoot() {
  addCors();
  // Prefer LittleFS index if present; else inline minimal page
  if (gServer.hasArg("raw")) {
    gServer.send(200, "text/plain", "esp32-s3-hid-bridge");
    return;
  }
  gServer.sendHeader("Location", "/index.html");
  gServer.send(302, "text/plain", "");
}

static void handleNotFound() {
  addCors();
  gServer.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
}

void httpBegin() {
  gServer.on("/v1/health", HTTP_GET, handleHealth);
  gServer.on("/v1/health", HTTP_OPTIONS, handleOptions);
  gServer.on("/v1/action", HTTP_POST, handleAction);
  gServer.on("/v1/action", HTTP_OPTIONS, handleOptions);
  gServer.on("/", HTTP_GET, handleRoot);
  gServer.onNotFound(handleNotFound);

  // Serve /index.html from LittleFS when uploaded via `pio run -t uploadfs`
  // Fallback: embedded minimal HTML if file missing
  gServer.on("/index.html", HTTP_GET, []() {
    addCors();
    static const char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 HID Bridge</title>
<style>
body{font-family:system-ui,sans-serif;max-width:720px;margin:2rem auto;padding:0 1rem}
button,input{font:inherit;margin:.25rem}
pre{background:#111;color:#0f0;padding:1rem;overflow:auto}
.row{display:flex;flex-wrap:wrap;gap:.5rem;margin:.5rem 0}
</style>
</head>
<body>
<h1>ESP32-S3 HID Bridge</h1>
<p>Manual test page. Device must be plugged into the host USB device port.</p>
<div class="row">
<button id="health">GET /v1/health</button>
<button id="click">click 960,540</button>
<button id="type">type hello</button>
<button id="enter">key enter</button>
</div>
<pre id="out">ready</pre>
<script>
const out=document.getElementById('out');
async function post(ops){
  const r=await fetch('/v1/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ops})});
  out.textContent=await r.text();
}
document.getElementById('health').onclick=async()=>{
  const r=await fetch('/v1/health'); out.textContent=await r.text();
};
document.getElementById('click').onclick=()=>post([{op:'click',x:960,y:540}]);
document.getElementById('type').onclick=()=>post([{op:'type',text:'hello'}]);
document.getElementById('enter').onclick=()=>post([{op:'key',key:'enter'}]);
</script>
</body>
</html>
)HTML";
    gServer.send_P(200, "text/html", kPage);
  });

  gServer.begin();
  Serial.printf("[http] listening on http://%s:%d\n", gWifiIp.c_str(), HTTP_PORT);
}

void httpLoop() {
  gServer.handleClient();
}
