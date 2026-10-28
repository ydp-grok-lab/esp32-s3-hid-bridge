#pragma once

// Optional local overrides (gitignored). Included FIRST so that every
// translation unit (main, wifi, hid, http) sees the same SCREEN_*/WIFI_* values.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Device identity
#ifndef DEVICE_NAME
#define DEVICE_NAME "esp32-s3-hid-bridge"
#endif

// SoftAP defaults (override password via secrets.h)
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "ESP32-HID-Bridge"
#endif
#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "hidbridge"
#endif
#ifndef WIFI_AP_CHANNEL
#define WIFI_AP_CHANNEL 6
#endif

// Optional STA: set WIFI_STA_SSID / WIFI_STA_PASSWORD in secrets.h
// If WIFI_STA_SSID is defined and non-empty, device prefers STA; falls back to SoftAP on failure.

// Logical screen size for absolute mouse mapping (click / drag / absolute move)
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 1920
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 1080
#endif

// HID absolute mouse uses 16-bit logical range in ESP32 USBHIDMouse abs API;
// we map [0..SCREEN_WIDTH/HEIGHT] -> [0..HID_ABS_MAX]
#ifndef HID_ABS_MAX
#define HID_ABS_MAX 32767
#endif

// HTTP
#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif

#ifndef MAX_OPS_PER_REQUEST
#define MAX_OPS_PER_REQUEST 64
#endif

#ifndef MAX_TYPE_CHARS
#define MAX_TYPE_CHARS 512
#endif

// Delays (ms) between composite HID gestures
#ifndef CLICK_PRESS_MS
#define CLICK_PRESS_MS 40
#endif
#ifndef DRAG_STEP_MS
#define DRAG_STEP_MS 8
#endif
#ifndef KEY_PRESS_MS
#define KEY_PRESS_MS 30
#endif

// Serial
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
