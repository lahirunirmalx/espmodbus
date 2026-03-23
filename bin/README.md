# Pre-built Binaries

Release binaries for Serial-to-Modbus Bridge v1.0.0

## Firmware

| Board | File | Size |
|-------|------|------|
| ESP32 (devkit) | `esp32/firmware.bin` | 304 KB |
| ESP32-C3 | `esp32-c3/firmware.bin` | 334 KB |

### Flash with esptool

```bash
# ESP32
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x10000 esp32/firmware.bin

# ESP32-C3
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x10000 esp32-c3/firmware.bin
```

Or use PlatformIO:

```bash
pio run -t upload -e esp32dev
pio run -t upload -e esp32-c3-devkitm-1
```

## GUI (Linux x64)

| Application | Description |
|-------------|-------------|
| `linux-x64/psu_gui` | Full GUI with VFD display, scope, keypad |
| `linux-x64/psu_gui_toolbar` | Compact toolbar version |

### Dependencies

```bash
sudo apt install libsdl2-2.0-0 libsdl2-ttf-2.0-0
```

### Run

```bash
./linux-x64/psu_gui /dev/ttyUSB0
./linux-x64/psu_gui_toolbar /dev/ttyUSB0
# Use --demo for testing without hardware
```
