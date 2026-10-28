#pragma once
/*
 * Minimal host-side Arduino shim: just enough of Arduino.h (String, Serial,
 * millis/delay, constrain) to compile hid_actions.cpp + action_handler.cpp on
 * a PC. delay() advances a virtual clock instead of sleeping, so tests can
 * assert timing (e.g. `wait` ops) instantly.
 */
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <algorithm>

#define PROGMEM
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

class String {
 public:
  String() {}
  String(const char* c) : s_(c ? c : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}

  unsigned int length() const { return (unsigned int)s_.size(); }
  const char* c_str() const { return s_.c_str(); }
  char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
  int indexOf(char c, unsigned int from = 0) const {
    size_t p = s_.find(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const String& t, unsigned int from = 0) const {
    size_t p = s_.find(t.s_, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(unsigned int from) const { return from >= s_.size() ? String() : String(s_.substr(from)); }
  String substring(unsigned int from, unsigned int to) const {
    if (from >= s_.size() || to <= from) return String();
    return String(s_.substr(from, to - from));
  }
  void trim() {
    size_t a = s_.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) { s_.clear(); return; }
    size_t b = s_.find_last_not_of(" \t\r\n");
    s_ = s_.substr(a, b - a + 1);
  }
  void toLowerCase() { for (auto& c : s_) c = (char)tolower((unsigned char)c); }
  // ArduinoJson (ARDUINOJSON_ENABLE_ARDUINO_STRING) writes via concat()/+=
  bool concat(const char* c) { s_ += c; return true; }
  bool concat(const char* c, unsigned int n) { s_.append(c, n); return true; }
  bool concat(char c) { s_ += c; return true; }
  bool reserve(unsigned int n) { s_.reserve(n); return true; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o; return *this; }
  String& operator+=(char o) { s_ += o; return *this; }
  String& operator=(const char* o) { s_ = o ? o : ""; return *this; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == o; }
  bool operator!=(const char* o) const { return s_ != o; }
  bool operator!=(const String& o) const { return s_ != o.s_; }
  friend String operator+(const String& a, const String& b) { return String(a.s_ + b.s_); }
  friend String operator+(const String& a, const char* b) { return String(a.s_ + b); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a) + b.s_); }
  const std::string& std() const { return s_; }

 private:
  std::string s_;
};

// ---- virtual clock + Serial capture -------------------------------------------------------
extern uint32_t g_millis;
extern std::string g_serial;  // everything printed to Serial
inline uint32_t millis() { return g_millis; }
inline void delay(uint32_t ms) { g_millis += ms; }

struct HostSerial {
  void begin(unsigned long) {}
  void print(const char* s) { g_serial += s; }
  void print(char c) { g_serial += c; }
  void println() { g_serial += '\n'; }
  void println(const char* s) { g_serial += s; g_serial += '\n'; }
  void println(const String& s) { g_serial += s.c_str(); g_serial += '\n'; }
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_serial += buf;
    return n;
  }
};
extern HostSerial Serial;
