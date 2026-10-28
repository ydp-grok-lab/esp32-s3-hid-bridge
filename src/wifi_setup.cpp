#include "wifi_setup.h"
#include "config.h"

#include <WiFi.h>

// Optional secrets (gitignored). If missing, SoftAP-only defaults apply.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

String gWifiMode = "ap";
String gWifiIp = "0.0.0.0";

static bool trySta() {
#if defined(WIFI_STA_SSID)
  const char* ssid = WIFI_STA_SSID;
  if (ssid == nullptr || ssid[0] == '\0') return false;

  const char* pass =
#if defined(WIFI_STA_PASSWORD)
      WIFI_STA_PASSWORD;
#else
      "";
#endif

  Serial.printf("[wifi] STA connecting to '%s'...\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  const uint32_t deadline = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    gWifiMode = "sta";
    gWifiIp = WiFi.localIP().toString();
    Serial.printf("[wifi] STA OK ip=%s\n", gWifiIp.c_str());
    return true;
  }

  Serial.println("[wifi] STA failed, falling back to SoftAP");
  WiFi.disconnect(true);
  delay(100);
  return false;
#else
  return false;
#endif
}

static void startAp() {
  const char* apPass =
#if defined(WIFI_AP_PASSWORD)
      WIFI_AP_PASSWORD;
#else
      "hidbridge";
#endif

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(WIFI_AP_SSID, apPass, WIFI_AP_CHANNEL);
  gWifiMode = "ap";
  gWifiIp = WiFi.softAPIP().toString();
  Serial.printf("[wifi] SoftAP %s ssid='%s' ip=%s\n",
                ok ? "OK" : "FAIL", WIFI_AP_SSID, gWifiIp.c_str());
}

void wifiBegin() {
  WiFi.persistent(false);
  if (!trySta()) {
    startAp();
  }
}
