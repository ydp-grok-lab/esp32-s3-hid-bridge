# Simulating / testing without hardware

Three layers, from cheapest to most realistic. None of them can test real USB HID output —
see "What cannot be verified".

| Layer | Command | Needs | Verifies |
|---|---|---|---|
| 1. Compile | `pio run -e esp32-s3-n8r8` (real firmware), `pio run -e wokwi` | PlatformIO | everything compiles/links for ESP32-S3 |
| 2. Host unit tests | `pio run -e native -t exec` or `CXX=g++ sh host_test/run.sh` | host g++ | JSON parsing, validation, op sequencing, key chords, coordinate mapping, clamps, wait timing, HID event order |
| 3. Wokwi simulation | `pio run -e wokwi` then `wokwi-cli .` | Wokwi account + token | real ESP32-S3 boot, serial log, Wi-Fi STA (Wokwi-GUEST), real WebServer/lwIP/ArduinoJson, in-firmware HTTP self-test |

## The `wokwi` build env

`[env:wokwi]` differs from the real env only where the simulator forces it:

* `-DHID_STUB` — `include/hid_stub.h` replaces `USBHIDAbsoluteMouse`/`USBHIDKeyboard`/`USB`; every HID call
  prints `[hid-stub] ...` on Serial. Wokwi does not simulate the ESP32-S3 USB-OTG device controller.
* `ARDUINO_USB_MODE=1` + `USB_CDC_ON_BOOT=1` — Serial becomes USB-Serial-JTAG (HWCDC), which is what Wokwi
  simulates (`"serialInterface": "USB_SERIAL_JTAG"` in `diagram.json`). The real env uses TinyUSB (mode 0).
* STA to the virtual open AP `Wokwi-GUEST` (channel 6 for faster connect). SoftAP is not reachable in Wokwi.
* `-DWOKWI_SELFTEST` — after boot a task calls the device's own API over `127.0.0.1` and prints
  `[selftest] PASS/FAIL ...`, ending with `[selftest] ALL PASSED`. This tests the whole HTTP stack
  without port forwarding (which `wokwi-cli` does not support).
* `merge_firmware.py` writes `.pio/build/wokwi/firmware.merged.bin` (bootloader + partitions + app, 8 MB flash),
  needed because an application-only image gets Wokwi's default 4 MB flash/partitions.

## Run it

```bash
pip install platformio
pio run -e wokwi
```

**Headless / CI (wokwi-cli, cloud simulation):**

```bash
curl -L https://wokwi.com/ci/install.sh | sh      # installs wokwi-cli
export WOKWI_CLI_TOKEN=...                         # create at https://wokwi.com/dashboard/ci (needs a Wokwi account, free)
wokwi-cli . --timeout 90000 \
  --expect-text '[selftest] ALL PASSED' --fail-text '[selftest] SOME FAILED' \
  --serial-log-file wokwi-serial.log
# or the scenario variant:
wokwi-cli . --scenario wokwi/automation.scenario.yaml --timeout 90000
```

Free tier: 50 simulated minutes per rolling 30 days, 5 min max per session. A run here takes well under a minute.

**Interactive (Wokwi for VS Code):** install the extension, `F1` → "Wokwi: Request a new License" (free account),
build `pio run -e wokwi`, `F1` → "Wokwi: Start Simulator". With a Private Gateway-capable plan, `[[net.forward]]` in
`wokwi.toml` exposes the simulated server at `http://localhost:8180` (`curl localhost:8180/v1/health`).
Without it, the simulator still runs and the self-test output appears in the serial monitor.

## What can be verified

* Build for ESP32-S3 N8R8 (Arduino, pioarduino), PSRAM/flash configuration compiles.
* Boot sequence, serial logs, `[wifi] STA OK`, `[http] listening`.
* `GET /v1/health` JSON; `POST /v1/action` happy paths for every op; all error paths
  (invalid JSON, missing `ops`, unknown op/key, too many ops, text too long); 404; OPTIONS 204; CORS headers.
* `wait` timing; coordinate scaling/clamping; chord press/release order (via `[hid-stub]` trace).

## What cannot be verified (needs the real board + a PC)

* USB HID enumeration, descriptor correctness, host-side acceptance of the absolute mouse / keyboard,
  actual cursor position/scale on a real OS, key layout behaviour. (Wokwi has no ESP32-S3 USB-OTG device/HID.)
* SoftAP mode (no clients in the simulator) and real-world Wi-Fi behaviour / range / power.
* Real PSRAM (octal) performance; real timing jitter.
* The `ARDUINO_USB_MODE=0` + `CDC_ON_BOOT=1` serial path (the sim env uses HWCDC instead).
