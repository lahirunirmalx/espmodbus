# Serial-to-Modbus Bridge (ESP32 + RS485)

USB Serial bridge for **dual-channel** Modbus RTU DC power supplies (e.g. SK120X-style modules): **36 V / 6 A** per channel. Uses **ESP32** or **ESP32-C3** and **RS485**.

No WiFi, no web UI—just a text-based serial protocol for direct computer control.

![SK120X Module](https://m.media-amazon.com/images/I/51RH2QGF4BL._AC_SL1500_.jpg)

---

## Features

- **USB Serial** — Text-based command/response protocol
- **Dual channel** — Two independent outputs (Modbus slave IDs 1 & 2), 36 V / 6 A per channel
- **Fast streaming** — Continuous DATA output at configurable rate (50-1000ms)
- **Board abstraction (HAL)** — One codebase for ESP32 and ESP32-C3

---

## Hardware Setup

- **MCU:** ESP32 or ESP32-C3 dev board
- **RS485** transceiver (DE/RE optional; enable via build flag)
- **Two** Modbus RTU power supply units (slave IDs 1 and 2)

### Pin mapping

| Board              | UART    | RX  | TX  | RS485 DE/RE |
|--------------------|---------|-----|-----|-------------|
| ESP32 (e.g. devkit)| Serial2 | 16  | 17  | 4           |
| ESP32-C3           | Serial1 | 20  | 21  | 3           |

Baud rate: **115200** (both USB CDC and Modbus).

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

Open Serial Monitor (115200) after upload. The device sends `!READY v1.0.0` on boot.

---

## Serial Protocol

The ESP32 **streams** data continuously. GUI/scripts send commands to control behavior.

### Output format (ESP32 → Host)

```
DATA <ch> setV=<v> setA=<a> outV=<v> outA=<a> outP=<p> outE=<e> inV=<v> temp=<t> time=<s> cap=<c> ovp=<v> ocp=<a> opp=<p> status=<s> cvcc=<m> out=<0|1> mppt=<0|1>
```

- `ch`: Channel number (1 or 2)
- Values are raw register values (see Register Map below)
- On read error: `DATA <ch> ... ERR`

### Commands (Host → ESP32)

| Command | Description |
|---------|-------------|
| `WRITE <ch> <reg> <val>` | Write single register. Response: `OK` or `ERR` |
| `LINK` | Copy Ch1 settings (V, I, output, MPPT) to Ch2. Response: `OK` or `ERR` |
| `STOP` | Stop streaming. Response: `OK stopped` |
| `START` | Resume streaming. Response: `OK started` |
| `POLL <ms>` | Set poll interval (50-1000ms). Response: `OK poll_ms=<ms>` |
| `POLL` | Query current poll interval. Response: `OK poll_ms=<ms>` |
| `RAW <ch>` | Dump all 32 registers. Response: `RAW <ch> <v0> <v1> ... <v31>` |
| `PING` | Connectivity check. Response: `PONG` |
| `REGS` | Print register map |
| `VERSION` | Firmware version. Response: `OK 1.0.0` |

- `ch`: 1 or 2
- `reg`/`val`: decimal or hex (`0x` prefix)

### Example session

```
!READY v1.0.0
DATA 1 setV=1200 setA=3000 outV=1198 outA=1500 outP=1797 outE=0 inV=2400 temp=381 time=0 cap=0 ovp=3700 ocp=6100 opp=25000 status=0 cvcc=0 out=1 mppt=0
DATA 2 setV=1200 setA=3000 outV=1195 outA=1480 outP=1768 outE=0 inV=2400 temp=378 time=0 cap=0 ovp=3700 ocp=6100 opp=25000 status=0 cvcc=0 out=1 mppt=0
> STOP
OK stopped
> WRITE 1 0x0000 1400
OK
DATA 1 setV=1400 ...
> POLL 200
OK poll_ms=200
> START
OK started
```

### Register Map

| Reg    | Name      | Unit        | R/W |
|--------|-----------|-------------|-----|
| 0x0000 | setV      | V * 100     | R/W |
| 0x0001 | setA      | A * 1000    | R/W |
| 0x0002 | outV      | V * 100     | R   |
| 0x0003 | outA      | A * 1000    | R   |
| 0x0004 | outP      | W * 100     | R   |
| 0x0005 | outE_H    | Wh (high)   | R   |
| 0x0006 | outE_L    | Wh (low)    | R   |
| 0x0007 | inV       | V * 100     | R   |
| 0x000D | temp      | °C * 10     | R   |
| 0x0009 | time_H    | sec (high)  | R   |
| 0x000A | time_L    | sec (low)   | R   |
| 0x000B | cap_H     | Ah*1000 (h) | R   |
| 0x000C | cap_L     | Ah*1000 (l) | R   |
| 0x000E | OVP       | V * 100     | R/W |
| 0x000F | OCP       | A * 1000    | R/W |
| 0x0010 | OPP       | W * 100     | R/W |
| 0x0012 | out       | 0/1         | R/W |
| 0x0013 | status    | flags       | R   |
| 0x0014 | cvcc      | 0=CV, 1=CC  | R   |
| 0x001F | mppt      | 0/1 (exp)   | R/W |

---

## GUI Application (psu-gui)

Standalone SDL2 control panel for the PSU with:

- **Dual-channel VFD display** — Realistic HP-style 7-segment readouts
- **Bar meters** — Voltage/Current per channel with real-time updates
- **Temperature gauge** — Warning indicators above 50°C
- **Dual-trace scope** — Voltage and current plots with auto-scaling
- **Common keypad** — Single numeric keypad for setting V/I on either channel

### Build & Run

```bash
cd psu-gui
make
./psu_gui /dev/ttyUSB0    # or --demo for testing
```

Requires: `libsdl2-dev`, `libsdl2-ttf-dev`

---

## Code Structure

- **`src/main.cpp`** — Serial protocol, DATA streaming, command parser
- **`src/hal/`** — Board HAL: UART/RS485 pins, init
- **`src/modbus_psu.h` / `modbus_psu.cpp`** — Register map, Modbus read/write
- **`psu-gui/`** — SDL2 GUI application

---

## Scripting Example (Python)

```python
import serial
import time
import re

ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
time.sleep(2)  # wait for ESP32 boot

# Wait for READY
while True:
    line = ser.readline().decode().strip()
    if line == '!READY':
        break

# Stop streaming to send commands
ser.write(b'STOP\n')
ser.readline()  # OK stopped

# Set voltage to 15.00V on channel 1
ser.write(b'WRITE 1 0x0000 1500\n')
print(ser.readline().decode().strip())  # OK

# Resume streaming and read data
ser.write(b'START\n')
ser.readline()  # OK started

# Parse DATA lines
for _ in range(10):
    line = ser.readline().decode().strip()
    if line.startswith('DATA'):
        m = re.search(r'outV=(\d+)', line)
        if m:
            print(f"Output voltage: {int(m.group(1)) / 100:.2f} V")

ser.close()
```

---

## Build Options

### RS485 Direction Control

By default, RS485 DE/RE control is disabled. To enable:

```ini
# In platformio.ini, add to build_flags:
build_flags =
    -DUSE_RS485_DIR=1
```

---

## Notes

- **MPPT** — Experimental; support depends on PSU firmware.
- Values are raw register values (setV 1200 = 12.00 V, setA 3000 = 3.000 A, etc.)
- Single-threaded Arduino environment; `modbus_psu_meaning()` / `modbus_psu_interpret()` use static buffers.

---

## Resources

- [SK120X DC Power Supply (Amazon)](https://www.amazon.com/SK120X-Regulated-Stabilized-Voltage-Converter/dp/B0F18HZD97)
- Modbus library: [lahirunirmalx/ModbusMaster](https://github.com/lahirunirmalx/ModbusMaster)

---

## License

MIT — use, modify, and share.
