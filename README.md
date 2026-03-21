# Serial-to-Modbus Bridge (ESP32 + RS485)

USB Serial bridge for **dual-channel** Modbus RTU DC power supplies (e.g. SK120X-style modules): **36 V / 6 A** per channel. Uses **ESP32** or **ESP32-C3** and **RS485**.

No WiFi, no web UI—just a text-based serial protocol for direct computer control.

![SK120X Module](https://m.media-amazon.com/images/I/51RH2QGF4BL._AC_SL1500_.jpg)

---

## Features

- **USB Serial** — Text-based command/response protocol
- **Dual channel** — Two independent outputs (Modbus slave IDs 1 & 2), 36 V / 6 A per channel
- **Simple protocol** — Human-readable commands; easy to script (Python, shell, etc.)
- **Board abstraction (HAL)** — One codebase for ESP32 and ESP32-C3

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

Open Serial Monitor (115200) after upload.

---

## Serial Protocol

All commands are **text, line-based** (terminated by `\n`). Responses start with `OK` or `ERR`.

| Command | Description |
|---------|-------------|
| `READ <ch> <reg>` | Read single register. Response: `OK <val>` |
| `READN <ch> <start> <count>` | Read count registers (max 32). Response: `OK <v0> <v1> ...` |
| `WRITE <ch> <reg> <val>` | Write single register. Response: `OK` |
| `STATUS <ch>` | Read full channel status. Response: `OK setV=<v> setA=<a> outV=<v> outA=<a> outP=<p> out=<0\|1> mppt=<0\|1>` |
| `LINK` | Copy Ch1 settings (V, I, output, MPPT) to Ch2. Response: `OK` |
| `HELP` | Print command list |

- `ch`: 1 or 2
- `reg`/`val`: decimal or hex (`0x` prefix)

### Example session

```
> STATUS 1
OK setV=1200 setA=3000 outV=1198 outA=1500 outP=1797 out=1 mppt=0

> READ 1 0x0000
OK 1200

> WRITE 1 0x0000 1400
OK

> LINK
OK
```

### Known registers

| Reg | Name | Unit |
|-----|------|------|
| 0x0000 | Vset | V * 100 |
| 0x0001 | Iset | A * 1000 |
| 0x0002 | Vout | V * 100 |
| 0x0003 | Iout | A * 1000 |
| 0x0004 | Pout | W * 100 |
| 0x0012 | Output Enable | 0/1 |
| 0x001F | MPPT Enable | 0/1 (exp) |

---

## GUI Application (psu-gui)

Standalone SDL2 control panel for the PSU with:

- **Dual-channel VFD display** — Realistic HP-style 7-segment readouts
- **Bar meters** — Voltage/Current per channel with real-time updates
- **Temperature gauge** — Warning indicators above 50°C
- **Dual-trace scope** — Voltage and current plots with auto-scaling
- **Common keypad** — Single numeric keypad for setting V/I on either channel

### Keypad Controls

| Control | Action |
|---------|--------|
| CH toggle | Switch target channel (CH1/CH2) |
| V/A toggle | Switch between Voltage and Current mode |
| Number keys | Enter setpoint value |
| C | Clear entry |
| < | Backspace |
| OK | Apply value to selected channel |

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `0-9`, `.` | Enter digits |
| `Tab` | Toggle channel (CH1/CH2) |
| `V` | Select voltage mode |
| `A` | Select current mode |
| `Enter` | Apply value |
| `Backspace` | Delete last digit |
| `C` or `Esc` | Clear entry |

### Build & Run

```bash
cd psu-gui
make
./psu_gui /dev/ttyUSB0    # or --demo for testing
```

Requires: `libsdl2-dev`, `libsdl2-ttf-dev`

---

## Code Structure

- **`src/main.cpp`** — Serial command parser, Modbus PSU calls
- **`src/hal/`** — Board HAL: UART/RS485 pins, init
- **`src/modbus_psu.h` / `modbus_psu.cpp`** — Register map, read/write
- **`psu-gui/`** — SDL2 GUI application

---

## Scripting Example (Python)

```python
import serial
import time

ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
time.sleep(2)  # wait for ESP32 boot

def send_cmd(cmd):
    ser.write((cmd + '\n').encode())
    return ser.readline().decode().strip()

print(send_cmd('STATUS 1'))
print(send_cmd('WRITE 1 0x0000 1500'))  # set 15.00V
print(send_cmd('STATUS 1'))

ser.close()
```

---

## Notes

- **MPPT** — Experimental; support depends on PSU firmware.
- Values are raw register values (Vset 1200 = 12.00 V, Iset 3000 = 3.000 A, etc.)

---

## Resources

- [SK120X DC Power Supply (Amazon)](https://www.amazon.com/SK120X-Regulated-Stabilized-Voltage-Converter/dp/B0F18HZD97)
- Modbus library: [lahirunirmalx/ModbusMaster](https://github.com/lahirunirmalx/ModbusMaster)

---

## License

MIT — use, modify, and share.
