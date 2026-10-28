#include <Arduino.h>

#include "config.h"
#include "hid_actions.h"
#include "wifi_setup.h"
#include "http_api.h"

#if __has_include("secrets.h")
#include "secrets.h"
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
}

void loop() {
  httpLoop();
  delay(1);
}
