# ESP32-S3 HID Bridge / ESP32-S3 USB HID 桥

ESP32-S3 **N8R8** (8MB flash + 8MB OPI PSRAM) firmware that:

1. Presents as a **USB HID mouse + keyboard** to a host (Android OTG phone, Mac, PC).
2. Exposes a **Wi‑Fi HTTP API** so an agent can send high-level input actions.

ESP32-S3 **N8R8**（8MB Flash + 8MB OPI PSRAM）固件：

1. 对主机（手机 OTG / Mac / PC）表现为 **USB HID 鼠标 + 键盘**。
2. 通过 **Wi‑Fi HTTP API** 接收上层 agent 下发的点击、拖拽、输入等动作。

---

## Hardware / 硬件

| Item | Notes |
|------|--------|
| Module | ESP32-S3 with **N8R8** (QIO flash + OPI PSRAM) |
| Board example | `esp32-s3-devkitc-1` class dual-USB boards |
| **USB device port** | Use the board port labeled **USB** / **OTG** (native USB D+/D−), **not** the UART “COM” port |
| Host | Phone with USB-OTG, or Mac/PC USB-C/A |

**Wiring notes / 接线注意**

- Plug **host ←→ ESP32 USB (OTG)** so the S3 enumerates as HID.
- On many DevKitC boards the second port is USB‑UART for flash/monitor only.
- You can flash via COM/UART, then connect the OTG port to the target host.
- SoftAP/STA is independent of USB; join Wi‑Fi from the controlling agent machine (or phone).

---

## Build & flash / 编译烧录

Requirements: [PlatformIO](https://platformio.org/) (CLI or VS Code/Cursor extension).

```bash
cd esp32-s3-hid-bridge

# Optional secrets (STA / AP password / screen size)
cp include/secrets.example.h include/secrets.h
# edit include/secrets.h

pio run -t upload          # flash firmware
pio run -t uploadfs        # optional: LittleFS data/index.html
pio device monitor         # 115200 baud serial log
```

PlatformIO env: `esp32-s3-n8r8`  
- `board_build.arduino.memory_type = qio_opi`  
- `ARDUINO_USB_MODE=0` (TinyUSB device)  
- `ARDUINO_USB_CDC_ON_BOOT=1` (Serial over native USB CDC)  
- Arduino-ESP32 **3.x** via pioarduino platform pin (for `USBHIDAbsoluteMouse`)

---

## Wi‑Fi / 无线

**Default SoftAP**

| SSID | Password (default) | AP IP |
|------|--------------------|-------|
| `ESP32-HID-Bridge` | `hidbridge` | usually `192.168.4.1` |

Join the AP from your laptop/phone, then call the API at `http://192.168.4.1/`.

**Optional STA**: in `include/secrets.h` define `WIFI_STA_SSID` / `WIFI_STA_PASSWORD`. Firmware tries STA for ~15s, then falls back to SoftAP.

---

## HTTP API / 接口约定

CORS: `Access-Control-Allow-Origin: *` on API routes.

### `GET /v1/health`

```json
{
  "ok": true,
  "device": "esp32-s3-hid-bridge",
  "uptime_ms": 12345,
  "ip": "192.168.4.1",
  "mode": "ap",
  "screen_width": 1920,
  "screen_height": 1080
}
```

`mode` is `"ap"` or `"sta"`. Extra `screen_*` fields document the logical coordinate space.

### `POST /v1/action`

Body:

```json
{
  "ops": [
    {"op": "click", "x": 100, "y": 200},
    {"op": "move", "x": 10, "y": -5, "relative": true},
    {"op": "drag", "x1": 10, "y1": 10, "x2": 200, "y2": 200},
    {"op": "scroll", "dy": -3},
    {"op": "type", "text": "hello"},
    {"op": "key", "key": "enter"},
    {"op": "wait", "ms": 100}
  ]
}
```

Success: `{"ok":true}`  
Failure: `{"ok":false,"error":"..."}` (HTTP 400)

#### Ops

| op | Fields | Behavior |
|----|--------|----------|
| `click` | `x`,`y` | Absolute move then left click |
| `move` | `x`,`y`, `relative`? | Absolute by default; `relative:true` adds delta to last known position |
| `drag` | `x1`,`y1`,`x2`,`y2` | Absolute drag with left button |
| `scroll` | `dy`, optional `dx` | Vertical/horizontal wheel (−127…127) |
| `type` | `text` | US QWERTY printable ASCII (max 512); non‑ASCII skipped |
| `key` | `key` | Named key or chord, e.g. `enter`, `tab`, `ctrl+c`, `cmd+space` |
| `wait` | `ms` | Delay (capped at 60s) |

**Logical screen** defaults to **1920×1080**, mapped to HID absolute range `0…32767`. Override with `SCREEN_WIDTH` / `SCREEN_HEIGHT` in `config.h` or `secrets.h`. Set this to the host display resolution for accurate clicks.

---

## curl examples / 示例

```bash
# SoftAP default
export BRIDGE=http://192.168.4.1

curl -s "$BRIDGE/v1/health" | jq .

curl -s -X POST "$BRIDGE/v1/action" \
  -H 'Content-Type: application/json' \
  -d '{"ops":[{"op":"click","x":960,"y":540}]}'

curl -s -X POST "$BRIDGE/v1/action" \
  -H 'Content-Type: application/json' \
  -d '{"ops":[
    {"op":"click","x":100,"y":200},
    {"op":"type","text":"hello"},
    {"op":"key","key":"enter"},
    {"op":"wait","ms":100},
    {"op":"move","x":10,"y":-5,"relative":true},
    {"op":"scroll","dy":-3},
    {"op":"drag","x1":10,"y1":10,"x2":200,"y2":200}
  ]}'
```

Browser: open `http://192.168.4.1/` (embedded test page) or `/index.html` after `uploadfs`.

---

## Keyboard mapping limits / 键盘限制

- Layout assumed: **US QWERTY**.
- `type` sends ASCII via `Keyboard.write`; UTF‑8 / CJK is **not** expanded to IME sequences.
- Named keys: `enter`, `tab`, `esc`, `backspace`, `delete`, arrows, `home`/`end`, `pageup`/`pagedown`, `f1`–`f12`, `space`, modifiers `ctrl`/`alt`/`shift`/`gui`(`win`/`cmd`/`meta`).
- Chords: `ctrl+c`, `ctrl+shift+esc`, `cmd+space`, etc.
- Mouse uses **absolute** HID reports (`USBHIDAbsoluteMouse`). Relative `move` is emulated from the last absolute position — if the host warps the cursor externally, relative deltas may drift until the next absolute `click`/`move`.

---

## Project layout / 目录

```
platformio.ini
include/config.h
include/secrets.example.h   # copy → secrets.h (gitignored)
include/hid_actions.h
include/wifi_setup.h
include/http_api.h
src/main.cpp
src/hid_actions.cpp
src/wifi_setup.cpp
src/http_api.cpp
data/index.html             # optional LittleFS test UI
LICENSE                     # MIT
README.md
.gitignore
```

---

## Simulation & tests / 仿真与测试

No board needed for compile checks, host unit tests (`pio run -e native -t exec`) and a Wokwi simulation
(`pio run -e wokwi`, `wokwi-cli .`). USB HID itself can only be verified on real hardware.
See [docs/WOKWI.md](docs/WOKWI.md).

---

## Push this repo / 推送

This tree is prepared for:

`https://github.com/ydp-grok-lab/esp32-s3-hid-bridge.git`

```bash
cd esp32-s3-hid-bridge
git remote add origin https://github.com/ydp-grok-lab/esp32-s3-hid-bridge.git   # if not set
git push -u origin main
```

---

## License

MIT — see [LICENSE](LICENSE).
