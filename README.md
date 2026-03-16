# Dual Output DC Power Supply — Web Controller (ESP32 + RS485 + Modbus)

Web-based controller for **dual-channel** Modbus RTU DC power supplies (e.g. SK120X-style modules): **36 V / 6 A** per channel. Uses **ESP32** or **ESP32-C3**, **RS485**, and **Modbus RTU**.

![SK120X Module](https://m.media-amazon.com/images/I/51RH2QGF4BL._AC_SL1500_.jpg)

---

## Features

- **WiFi** — Station mode or fallback AP (`SK120x-ESP32`); automatic reconnect if connection drops
- **Dual channel** — Two independent outputs (Modbus slave IDs 1 & 2), 36 V / 6 A per channel
- **Real-time sync** — WebSocket (port 81) pushes device changes to the UI; writes can go over WebSocket for low latency
- **Live monitoring** — Set/Output voltage & current, power, output ON/OFF, MPPT (experimental)
- **Remote control** — Voltage & current setpoints, output toggle, MPPT; **TRACKING** copies Ch1 settings to Ch2
- **Embedded UI** — Single-page HTML/CSS/JS (no CDN), VFD-style readouts, gauges, scope traces
- **Register scanner** — Read holding registers by range (max 32 per request), CSV export
- **Configurable poll rate** — Background Modbus poll interval (50–5000 ms), stored in NVS
- **Health & config API** — `/api/health` (stats), `/api/config` (poll rate)
- **Board abstraction (HAL)** — One codebase for ESP32 and ESP32-C3; pin mapping in `hal/hal_board.cpp`

---

## Web Interface Preview

![Dual Output DC Power Supply — Web UI (Demo)](data/demo_ui.png)

- **Header:** Dual Output DC Power Supply, 36 V / 6 A badge, ONLINE/ERROR status
- **System control:** TRACKING (link Ch1→Ch2), REFRESH, EXPORT
- **Per channel:** VFD display (V, A, W), OUTPUT toggle, voltage/current knobs & SET, analog gauges, scope trace
- **Scanner:** CH1/CH2 selector, START/END registers (max 32 regs), SCAN, CSV export

UI is self-contained; works offline once loaded. Status updates arrive via WebSocket when the device state changes.

---

## Hardware Setup

- **MCU:** ESP32 or ESP32-C3 dev board
- **RS485** transceiver (DE/RE optional; configurable in HAL)
- **Two** Modbus RTU power supply units (slave IDs 1 and 2)

### Pin mapping

| Board              | UART    | RX  | TX  | RS485 DE/RE |
|--------------------|---------|-----|-----|-------------|
| ESP32 (e.g. devkit)| Serial2 | 16  | 17  | 4           |
| ESP32-C3           | Serial1 | 20  | 21  | 3           |

Baud rate: **115200**.

---

## Software Setup

### Build & upload (PlatformIO)

```bash
# Build both environments
pio run

# Build for a specific board
pio run -e esp32dev
pio run -e esp32-c3-devkitm-1

# Upload
pio run -t upload
```

### WiFi

Set credentials in `src/main.cpp`:

```cpp
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

After upload, open Serial Monitor (115200) for the device IP, or connect to AP **SK120x-ESP32** and open the AP IP.

---

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Web UI (HTML) |
| GET | `/api/status` | Combined JSON status for both channels (ch1, ch2). Served from cache; updated by background Modbus task. |
| GET | `/api/status/1` | Same as `/api/status` (returns combined response) |
| GET | `/api/status/2` | Same as `/api/status` (returns combined response) |
| POST | `/api/write/1?reg=X&val=Y` | Write register on channel 1. Queued to Modbus task. |
| POST | `/api/write/2?reg=X&val=Y` | Write register on channel 2. Queued to Modbus task. |
| GET | `/api/scan/1?start=A&end=B` | Scan holding registers, channel 1. **Range max 32 registers** (e.g. start=0, end=31). |
| GET | `/api/scan/2?start=A&end=B` | Scan holding registers, channel 2. **Range max 32 registers**. |
| POST | `/api/link` | Copy Ch1 settings (V, I, output, MPPT) to Ch2. Queued to Modbus task. |
| GET | `/api/health` | System stats: `uptime_s`, `modbus_ok`, `modbus_fail`, `ws_connects`, `ws_disconnects`, `heap_free`. |
| GET | `/api/config` | Current config: `{"ok":true,"poll_ms":400}`. |
| POST | `/api/config?poll_ms=200` | Set background poll interval (50–5000 ms). Persisted in NVS. Returns `{"ok":true,"poll_ms":200}`. |

### WebSocket (port 81)

- **Connect:** `ws://<device-ip>:81/`
- **Server → client:** Pushes full status JSON whenever Modbus values change: `{"ch1":{...},"ch2":{...}}`
- **Client → server (write):** Send `{"cmd":"write","ch":1,"reg":0,"val":1200}`. Server replies `{"cmd":"write","ok":true}` and broadcasts updated status.
- The UI uses WebSocket for live updates and for writes when connected; it falls back to HTTP GET/POST otherwise.

---

## Code Structure

- **`src/main.cpp`** — WiFi (with reconnect), wiring of HAL, Modbus driver, queue, task, HTTP server, WebSocket init; `loop`: check_wifi, app_ws_loop, handleClient, app_http_background_poll
- **`src/hal/`** — Board HAL: UART/RS485 pins, init, per-board `#if` (ESP32 vs ESP32-C3)
- **`src/modbus_psu.h` / `modbus_psu.cpp`** — Register map, read/write, slave ID for channel; single 32-reg batch read used by Modbus task
- **`src/modbus_queue.h` / `modbus_queue.cpp`** — Write-priority command queue (used when not using FreeRTOS task path for legacy compatibility)
- **`src/modbus_task.h` / `modbus_task.cpp`** — FreeRTOS task: processes write/link queue, background poll, thread-safe status cache; drives real-time updates
- **`src/app_http.h` / `app_http.cpp`** — HTTP handlers (status, write, scan, link, health, config), WebSocket server (port 81), broadcast on status change
- **`src/index_html.h`** — Embedded UI (PROGMEM); WebSocket client, write via WS or HTTP fallback

Adding a new board: extend `hal_board.cpp` with a new `#if` block; no changes in app or Modbus code.

---

## Architecture (summary)

- **Modbus task** (FreeRTOS): Runs on its own task; executes write/link commands (priority) and periodic 32-reg reads per channel; updates a shared status JSON and a “changed” flag.
- **Main loop:** Checks WiFi, runs WebSocket loop, handles HTTP, then checks Modbus task “changed” flag and broadcasts status to all WebSocket clients.
- **Single batch read:** Each channel is read with one Modbus transaction (registers 0x00–0x1F); setV, setA, outV, outA, outP, outE, mppt are extracted from that batch.

---

## Notes

- **MPPT** — Experimental; support depends on PSU firmware.
- **Register scanner** — Use to discover and inspect unknown registers. Limited to 32 registers per request to avoid long blocks.
- **Demo UI** — `data/demo.html` and `data/index.html` mirror the embedded UI for local preview.

---

## Resources

- [SK120X DC Power Supply (Amazon)](https://www.amazon.com/SK120X-Regulated-Stabilized-Voltage-Converter/dp/B0F18HZD97)
- Modbus library: [lahirunirmalx/ModbusMaster](https://github.com/lahirunirmalx/ModbusMaster)
- WebSocket library: [links2004/WebSockets](https://github.com/Links2004/arduinoWebSockets)
- JSON: [ArduinoJson](https://arduinojson.org/)

---

## License

MIT — use, modify, and share.
