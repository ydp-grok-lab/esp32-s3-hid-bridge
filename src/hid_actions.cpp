#include "hid_actions.h"
#include "config.h"

#include "USB.h"
#include "USBHIDMouse.h"
#include "USBHIDKeyboard.h"

// Absolute mouse: one HID mouse interface. Relative moves are applied as
// deltas against the last absolute logical position (see README limits).
static USBHIDAbsoluteMouse Mouse;
static USBHIDKeyboard Keyboard;
static bool gHidReady = false;
static int32_t gCurX = SCREEN_WIDTH / 2;
static int32_t gCurY = SCREEN_HEIGHT / 2;

static int16_t toHidX(int32_t x) {
  if (x < 0) x = 0;
  if (x > SCREEN_WIDTH) x = SCREEN_WIDTH;
  return (int16_t)((int64_t)x * HID_ABS_MAX / SCREEN_WIDTH);
}

static int16_t toHidY(int32_t y) {
  if (y < 0) y = 0;
  if (y > SCREEN_HEIGHT) y = SCREEN_HEIGHT;
  return (int16_t)((int64_t)y * HID_ABS_MAX / SCREEN_HEIGHT);
}

static void absMove(int32_t x, int32_t y, int8_t wheel = 0, int8_t pan = 0) {
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x > SCREEN_WIDTH) x = SCREEN_WIDTH;
  if (y > SCREEN_HEIGHT) y = SCREEN_HEIGHT;
  gCurX = x;
  gCurY = y;
  Mouse.move(toHidX(x), toHidY(y), wheel, pan);
}

void hidBegin() {
  Mouse.begin();
  Keyboard.begin();
  USB.begin();
  gHidReady = true;
  // Park cursor near center so first relative moves have a known origin
  absMove(gCurX, gCurY);
  Serial.println("[hid] USB HID absolute mouse + keyboard started (device mode)");
  Serial.printf("[hid] logical screen %dx%d -> HID abs 0..%d\n",
                SCREEN_WIDTH, SCREEN_HEIGHT, HID_ABS_MAX);
}

void hidMoveRelative(int16_t dx, int16_t dy) {
  if (!gHidReady) return;
  absMove(gCurX + dx, gCurY + dy);
}

void hidMoveAbsolute(int32_t x, int32_t y) {
  if (!gHidReady) return;
  absMove(x, y);
}

void hidClick(int32_t x, int32_t y, uint8_t button) {
  if (!gHidReady) return;
  absMove(x, y);
  delay(15);
  Mouse.press(button);
  delay(CLICK_PRESS_MS);
  Mouse.release(button);
}

void hidDrag(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t button) {
  if (!gHidReady) return;
  absMove(x1, y1);
  delay(20);
  Mouse.press(button);
  delay(CLICK_PRESS_MS);

  const int steps = 24;
  for (int i = 1; i <= steps; i++) {
    int32_t xi = x1 + (int32_t)((int64_t)(x2 - x1) * i / steps);
    int32_t yi = y1 + (int32_t)((int64_t)(y2 - y1) * i / steps);
    absMove(xi, yi);
    delay(DRAG_STEP_MS);
  }

  Mouse.release(button);
}

void hidScroll(int8_t dy, int8_t dx) {
  if (!gHidReady) return;
  // Keep position; send wheel/pan on absolute report
  Mouse.move(toHidX(gCurX), toHidY(gCurY), dy, dx);
}

void hidTypeText(const String& text) {
  if (!gHidReady) return;
  size_t n = text.length();
  if (n > MAX_TYPE_CHARS) n = MAX_TYPE_CHARS;
  for (size_t i = 0; i < n; ) {
    char c = text.charAt(i);
    if ((uint8_t)c >= 0x80) {
      // Skip UTF-8 continuation / non-ASCII code units
      i++;
      while (i < n && ((uint8_t)text.charAt(i) & 0xC0) == 0x80) i++;
      continue;
    }
    Keyboard.write((uint8_t)c);
    delay(5);
    i++;
  }
}

struct NamedKey {
  const char* name;
  uint8_t key;
  bool modifier;
};

static const NamedKey kNamedKeys[] = {
  {"enter", KEY_RETURN, false},
  {"return", KEY_RETURN, false},
  {"tab", KEY_TAB, false},
  {"esc", KEY_ESC, false},
  {"escape", KEY_ESC, false},
  {"backspace", KEY_BACKSPACE, false},
  {"delete", KEY_DELETE, false},
  {"del", KEY_DELETE, false},
  {"insert", KEY_INSERT, false},
  {"home", KEY_HOME, false},
  {"end", KEY_END, false},
  {"pageup", KEY_PAGE_UP, false},
  {"pagedown", KEY_PAGE_DOWN, false},
  {"up", KEY_UP_ARROW, false},
  {"down", KEY_DOWN_ARROW, false},
  {"left", KEY_LEFT_ARROW, false},
  {"right", KEY_RIGHT_ARROW, false},
  {"space", ' ', false},
  {"f1", KEY_F1, false},
  {"f2", KEY_F2, false},
  {"f3", KEY_F3, false},
  {"f4", KEY_F4, false},
  {"f5", KEY_F5, false},
  {"f6", KEY_F6, false},
  {"f7", KEY_F7, false},
  {"f8", KEY_F8, false},
  {"f9", KEY_F9, false},
  {"f10", KEY_F10, false},
  {"f11", KEY_F11, false},
  {"f12", KEY_F12, false},
  {"ctrl", KEY_LEFT_CTRL, true},
  {"control", KEY_LEFT_CTRL, true},
  {"alt", KEY_LEFT_ALT, true},
  {"shift", KEY_LEFT_SHIFT, true},
  {"gui", KEY_LEFT_GUI, true},
  {"win", KEY_LEFT_GUI, true},
  {"cmd", KEY_LEFT_GUI, true},
  {"meta", KEY_LEFT_GUI, true},
};

static String lowerCopy(const String& s) {
  String o = s;
  o.toLowerCase();
  return o;
}

bool hidKeyNamed(const String& keyName) {
  if (!gHidReady) return false;
  String k = lowerCopy(keyName);
  k.trim();

  if (k.indexOf('+') >= 0) {
    uint8_t mods[4];
    int modCount = 0;
    uint8_t finalKey = 0;
    bool hasFinal = false;

    int start = 0;
    while (start <= (int)k.length()) {
      int plus = k.indexOf('+', start);
      String part = (plus < 0) ? k.substring(start) : k.substring(start, plus);
      part.trim();
      start = (plus < 0) ? (int)k.length() + 1 : plus + 1;
      if (part.length() == 0) continue;

      bool found = false;
      for (size_t i = 0; i < sizeof(kNamedKeys) / sizeof(kNamedKeys[0]); i++) {
        if (part == kNamedKeys[i].name) {
          if (kNamedKeys[i].modifier) {
            if (modCount < 4) mods[modCount++] = kNamedKeys[i].key;
          } else {
            finalKey = kNamedKeys[i].key;
            hasFinal = true;
          }
          found = true;
          break;
        }
      }
      if (!found && part.length() == 1) {
        finalKey = (uint8_t)part.charAt(0);
        hasFinal = true;
        found = true;
      }
      if (!found) return false;
    }

    for (int i = 0; i < modCount; i++) Keyboard.press(mods[i]);
    if (hasFinal) {
      Keyboard.press(finalKey);
      delay(KEY_PRESS_MS);
      Keyboard.release(finalKey);
    } else {
      delay(KEY_PRESS_MS);
    }
    for (int i = modCount - 1; i >= 0; i--) Keyboard.release(mods[i]);
    return true;
  }

  for (size_t i = 0; i < sizeof(kNamedKeys) / sizeof(kNamedKeys[0]); i++) {
    if (k == kNamedKeys[i].name) {
      Keyboard.press(kNamedKeys[i].key);
      delay(KEY_PRESS_MS);
      Keyboard.release(kNamedKeys[i].key);
      return true;
    }
  }
  if (k.length() == 1) {
    Keyboard.write((uint8_t)k.charAt(0));
    return true;
  }
  return false;
}

void hidWait(uint32_t ms) {
  if (ms > 60000) ms = 60000;
  delay(ms);
}

bool hidExecOp(const String& op,
               int32_t x, int32_t y,
               int32_t x1, int32_t y1, int32_t x2, int32_t y2,
               int32_t dy, int32_t dx,
               bool relative,
               const String& text,
               const String& key,
               uint32_t ms,
               String& errOut) {
  String o = lowerCopy(op);
  o.trim();

  if (o == "click") {
    hidClick(x, y);
    return true;
  }
  if (o == "move") {
    if (relative) {
      hidMoveRelative((int16_t)constrain(x, -32768, 32767),
                      (int16_t)constrain(y, -32768, 32767));
    } else {
      hidMoveAbsolute(x, y);
    }
    return true;
  }
  if (o == "drag") {
    hidDrag(x1, y1, x2, y2);
    return true;
  }
  if (o == "scroll") {
    int8_t sdy = (int8_t)constrain(dy, -127, 127);
    int8_t sdx = (int8_t)constrain(dx, -127, 127);
    hidScroll(sdy, sdx);
    return true;
  }
  if (o == "type") {
    if ((int)text.length() > MAX_TYPE_CHARS) {
      errOut = "text too long";
      return false;
    }
    hidTypeText(text);
    return true;
  }
  if (o == "key") {
    if (!hidKeyNamed(key)) {
      errOut = String("unknown key: ") + key;
      return false;
    }
    return true;
  }
  if (o == "wait") {
    hidWait(ms);
    return true;
  }

  errOut = String("unknown op: ") + op;
  return false;
}
