#!/bin/sh
# Plain-compiler fallback for the host tests (no PlatformIO needed).
#   sh host_test/run.sh                       # uses $CXX or g++
#   CXX="python3 -m ziglang c++" sh host_test/run.sh
# ArduinoJson: set ARDUINOJSON_DIR to the dir containing ArduinoJson.h
#   (e.g. .pio/libdeps/native/ArduinoJson/src after `pio run -e native`,
#    or `git clone --depth 1 https://github.com/bblanchon/ArduinoJson`).
set -e
cd "$(dirname "$0")/.."
CXX=${CXX:-g++}
AJ=${ARDUINOJSON_DIR:-$(ls -d .pio/libdeps/*/ArduinoJson/src 2>/dev/null | head -1)}
[ -n "$AJ" ] || { echo "ArduinoJson not found; set ARDUINOJSON_DIR"; exit 2; }
mkdir -p .pio/host
$CXX -std=gnu++17 -Wall -DHID_STUB -DNATIVE_TEST -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
  -Ihost_test/shim -Iinclude -I"$AJ" \
  host_test/test_main.cpp src/action_handler.cpp src/hid_actions.cpp -o .pio/host/test_actions
.pio/host/test_actions
