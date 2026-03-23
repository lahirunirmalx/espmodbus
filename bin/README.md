# Pre-built Binaries

## ESP32 Firmware

| File | Board | Description |
|------|-------|-------------|
| `esp32/firmware.bin` | ESP32 DevKit | Standard ESP32 (Xtensa) |
| `esp32-c3/firmware.bin` | ESP32-C3 | RISC-V variant |

### Flashing

```bash
# ESP32
esptool.py --port /dev/ttyUSB0 write_flash 0x10000 esp32/firmware.bin

# ESP32-C3
esptool.py --port /dev/ttyACM0 write_flash 0x10000 esp32-c3/firmware.bin
```

Or use PlatformIO: `pio run -t upload`

## GUI Applications (Linux x64)

| File | Description |
|------|-------------|
| `linux-x64/psu_gui` | Full SDL2 GUI with VFD display, gauges, scope |
| `linux-x64/psu_gui_toolbar` | Compact toolbar-style GUI |

### Running

```bash
# With device
./linux-x64/psu_gui /dev/ttyUSB0

# Demo mode
./linux-x64/psu_gui
```

Requires: `libsdl2-2.0-0`, `libsdl2-ttf-2.0-0`
