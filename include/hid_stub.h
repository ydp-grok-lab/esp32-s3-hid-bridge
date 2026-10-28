#pragma once
/*
 * HID_STUB backend: replaces USBHIDAbsoluteMouse / USBHIDKeyboard with
 * logging stubs so the firmware logic can run where no USB-OTG device
 * controller exists (Wokwi simulator, host-side unit tests).
 *
 * Enabled with -DHID_STUB (see [env:wokwi] and [env:native] in platformio.ini).
 * The real firmware env (esp32-s3-n8r8) never compiles this file.
 *
 * Every call prints one "[hid-stub] ..." line on Serial so tests / the Wokwi
 * CLI can assert on the exact HID events that WOULD have been sent.
 */
#include <Arduino.h>

// Key codes copied from Arduino-ESP32 3.x USBHIDKeyboard.h
#ifndef KEY_LEFT_CTRL
#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_UP_ARROW    0xDA
#define KEY_DOWN_ARROW  0xD9
#define KEY_LEFT_ARROW  0xD8
#define KEY_RIGHT_ARROW 0xD7
#define KEY_BACKSPACE   0xB2
#define KEY_TAB         0xB3
#define KEY_RETURN      0xB0
#define KEY_ESC         0xB1
#define KEY_INSERT      0xD1
#define KEY_DELETE      0xD4
#define KEY_PAGE_UP     0xD3
#define KEY_PAGE_DOWN   0xD6
#define KEY_HOME        0xD2
#define KEY_END         0xD5
#define KEY_F1          0xC2
#define KEY_F2          0xC3
#define KEY_F3          0xC4
#define KEY_F4          0xC5
#define KEY_F5          0xC6
#define KEY_F6          0xC7
#define KEY_F7          0xC8
#define KEY_F8          0xC9
#define KEY_F9          0xCA
#define KEY_F10         0xCB
#define KEY_F11         0xCC
#define KEY_F12         0xCD
#endif

#ifndef MOUSE_LEFT
#define MOUSE_LEFT 1
#endif

class USBHIDAbsoluteMouse {
 public:
  void begin() { Serial.println("[hid-stub] mouse.begin"); }
  void move(int16_t x, int16_t y, int8_t wheel = 0, int8_t pan = 0) {
    Serial.printf("[hid-stub] mouse.move x=%d y=%d wheel=%d pan=%d\n", x, y, wheel, pan);
  }
  void press(uint8_t b = MOUSE_LEFT) { Serial.printf("[hid-stub] mouse.press b=%u\n", b); }
  void release(uint8_t b = MOUSE_LEFT) { Serial.printf("[hid-stub] mouse.release b=%u\n", b); }
};

class USBHIDKeyboard {
 public:
  void begin() { Serial.println("[hid-stub] keyboard.begin"); }
  size_t press(uint8_t k) { Serial.printf("[hid-stub] key.press 0x%02X\n", k); return 1; }
  size_t release(uint8_t k) { Serial.printf("[hid-stub] key.release 0x%02X\n", k); return 1; }
  size_t write(uint8_t c) { Serial.printf("[hid-stub] key.write 0x%02X\n", c); return 1; }
};

// Mirrors USB.begin() from the real core.
struct HidStubUsb {
  void begin() { Serial.println("[hid-stub] USB.begin (no USB device in this build)"); }
};
static HidStubUsb USB;
