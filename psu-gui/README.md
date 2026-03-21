# PSU Control GUI — SDL2 Application

Standalone C GUI for controlling dual-channel DC power supplies via serial-to-Modbus bridge (ESP32).

Mimics the web UI: VFD-style displays, analog gauges, scope traces, output toggles, setpoint controls.

---

## Features

- **VFD display** — Voltage, current, power readouts with green-on-black style
- **Analog gauges** — Needle gauges for V/A
- **Scope traces** — Real-time voltage waveform per channel
- **Controls** — OUTPUT toggle, voltage/current setpoints
- **TRACKING** — Link Ch1 settings to Ch2
- **Demo mode** — Runs with simulated data if serial port unavailable

---

## Dependencies

Install SDL2 and SDL2_ttf:

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev

# Fedora
sudo dnf install SDL2-devel SDL2_ttf-devel

# Arch
sudo pacman -S sdl2 sdl2_ttf
```

Also needs a monospace font (DejaVu Sans Mono or Liberation Mono).

---

## Build

```bash
make
```

---

## Run

```bash
# With ESP32 connected
./psu_gui /dev/ttyUSB0

# Demo mode (no device)
./psu_gui
```

If the serial port is unavailable, the app runs in **DEMO mode** with simulated data.

---

## Serial Protocol

The GUI communicates with the ESP32 using the text-based serial protocol:

| Command | Description |
|---------|-------------|
| `STATUS <ch>` | Read channel status |
| `WRITE <ch> <reg> <val>` | Write register |
| `LINK` | Copy Ch1 to Ch2 |

See the ESP32 firmware README for full protocol details.

---

## Controls

- **OUTPUT button** — Toggle output on/off
- **VOLTAGE/CURRENT inputs** — Click to edit, type value, press SET or Enter
- **TRACKING** — When enabled, copies Ch1 settings to Ch2
- **REFRESH** — Force status poll

---

## Code Structure

- **`main.c`** — SDL2 GUI, event handling, rendering
- **`serial_port.c/h`** — Linux serial port abstraction
- **`psu_protocol.c/h`** — PSU command/response parsing

---

## License

MIT
