#pragma once

#include <Arduino.h>

void hidBegin();
void hidMoveRelative(int16_t dx, int16_t dy);
void hidMoveAbsolute(int32_t x, int32_t y);
void hidClick(int32_t x, int32_t y, uint8_t button = 1);
void hidDrag(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t button = 1);
void hidScroll(int8_t dy, int8_t dx = 0);
void hidTypeText(const String& text);
bool hidKeyNamed(const String& keyName);
void hidWait(uint32_t ms);

// Execute one JSON-like op already parsed into fields. Returns false on error.
// op: click|move|drag|scroll|type|key|wait
bool hidExecOp(const String& op,
               int32_t x, int32_t y,
               int32_t x1, int32_t y1, int32_t x2, int32_t y2,
               int32_t dy, int32_t dx,
               bool relative,
               const String& text,
               const String& key,
               uint32_t ms,
               String& errOut);
