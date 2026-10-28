#pragma once

#include <Arduino.h>
#include <WebServer.h>

extern WebServer gServer;

void httpBegin();
void httpLoop();
