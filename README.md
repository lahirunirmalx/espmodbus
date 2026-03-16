# Dual Output DC Power Supply — Web Controller (ESP32 + RS485 + Modbus)

Web-based controller for **dual-channel** Modbus RTU DC power supplies (e.g. SK120X-style modules): **36 V / 6 A** per channel. Uses **ESP32** or **ESP32-C3**, **RS485**, and **Modbus RTU**.

![SK120X Module](https://m.media-amazon.com/images/I/51RH2QGF4BL._AC_SL1500_.jpg)

---

## Features

- **WiFi** — Station mode or fallback AP (`SK120x-ESP32`)
- **Dual channel** — Two independent outputs (Modbus slave IDs 1 & 2), 36 V / 6 A per channel
- **Live monitoring** — Set/Output voltage & current, power, output ON/OFF, MPPT (experimental)
- **Remote control** — Voltage & current setpoints, output toggle, MPPT; **TRACKING** copies Ch1 settings to Ch2
- **Embedded UI** — Single-page HTML/CSS/JS (no CDN), VFD-style readouts, gauges, scope traces
- **Register scanner** — Read holding registers by range, CSV export
- **Board abstraction (HAL)** — One codebase for ESP32 and ESP32-C3; pin mapping in `hal/hal_board.cpp`

---

## Web Interface Preview

![Dual Output DC Power Supply — Web UI (Demo)](data/demo_ui.png)

- **Header:** Dual Output DC Power Supply, 36 V / 6 A badge, ONLINE/ERROR status
- **System control:** TRACKING (link Ch1→Ch2), REFRESH, EXPORT
- **Per channel:** VFD display (V, A, W), OUTPUT toggle, voltage/current knobs & SET, analog gauges, scope trace
- **Scanner:** CH1/CH2 selector, START/END registers, SCAN, CSV export

UI is self-contained; works offline once loaded.

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
| GET | `/api/status/1` | JSON status for channel 1 |
| GET | `/api/status/2` | JSON status for channel 2 |
| POST | `/api/write/1?reg=X&val=Y` | Write register on channel 1 |
| POST | `/api/write/2?reg=X&val=Y` | Write register on channel 2 |
| GET | `/api/scan/1?start=A&end=B` | Scan holding registers, channel 1 |
| GET | `/api/scan/2?start=A&end=B` | Scan holding registers, channel 2 |
| POST | `/api/link` | Copy Ch1 settings (V, I, output, MPPT) to Ch2 |

---

## Code Structure

- **`src/main.cpp`** — WiFi, wiring of HAL, Modbus driver, HTTP server
- **`src/hal/`** — Board HAL: UART/RS485 pins, init, per-board `#if` (ESP32 vs ESP32-C3)
- **`src/modbus_psu.h` / `modbus_psu.cpp`** — Register map, read/write, slave ID for channel
- **`src/app_http.h` / `app_http.cpp`** — HTTP handlers (status, write, scan, link)
- **`src/index_html.h`** — Embedded UI (PROGMEM)

Adding a new board: extend `hal_board.cpp` with a new `#if` block; no changes in app or Modbus code.

---

## Notes

- **MPPT** — Experimental; support depends on PSU firmware.
- **Register scanner** — Use to discover and inspect unknown registers.
- **Demo UI** — `data/demo.html` and `data/index.html` mirror the embedded UI for local preview.

---

## Resources

- [SK120X DC Power Supply (Amazon)](https://www.amazon.com/SK120X-Regulated-Stabilized-Voltage-Converter/dp/B0F18HZD97)
- Modbus library: [lahirunirmalx/ModbusMaster](https://github.com/lahirunirmalx/ModbusMaster)

---

## License

MIT — use, modify, and share.
