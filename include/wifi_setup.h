#pragma once

#include <Arduino.h>

// "ap" or "sta"
extern String gWifiMode;
extern String gWifiIp;

void wifiBegin();
