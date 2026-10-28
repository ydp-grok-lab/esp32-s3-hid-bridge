#pragma once

#include <Arduino.h>

// Pure request logic of the HTTP API, separated from WebServer so it can be
// unit-tested on the host (see test/host) and exercised in the simulator.

// Body of GET /v1/health. Always HTTP 200.
String actionBuildHealthJson();

// Body of POST /v1/action: parses JSON, validates, executes ops via hidExecOp().
// Writes the JSON response into `out` and returns the HTTP status (200 or 400).
int actionHandleBody(const String& body, String& out);
