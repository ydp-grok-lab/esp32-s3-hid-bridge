#include <Arduino.h>

#include "config.h"
#include "hid_actions.h"
#include "wifi_setup.h"
#include "http_api.h"

#ifdef WOKWI_SELFTEST
#include "wokwi_selftest.h"
#endif

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println("========================================");
  Serial.println(" ESP32-S3 USB HID Bridge");
  Serial.println(" device: " DEVICE_NAME);
  Serial.println("========================================");

  hidBegin();
  wifiBegin();
  httpBegin();

  Serial.println("[boot] ready");

#ifdef WOKWI_SELFTEST
  wokwiSelftestBegin();
#endif
}

void loop() {
  httpLoop();
  delay(1);
}
