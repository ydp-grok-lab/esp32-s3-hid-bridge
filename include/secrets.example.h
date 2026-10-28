#pragma once
/*
 * Copy this file to secrets.h (gitignored) and fill in values.
 * SoftAP password can stay as the default in config.h if you prefer.
 *
 *   cp include/secrets.example.h include/secrets.h
 */

// SoftAP password (min 8 chars for WPA2). Leave undefined to use config.h default.
// #define WIFI_AP_PASSWORD "your-ap-password"

// Optional station mode: join an existing Wi-Fi network.
// When WIFI_STA_SSID is set, firmware tries STA first, then SoftAP fallback.
// #define WIFI_STA_SSID     "your-home-ssid"
// #define WIFI_STA_PASSWORD "your-home-password"

// Optional: override logical screen size used for absolute coords
// #define SCREEN_WIDTH  1920
// #define SCREEN_HEIGHT 1080
