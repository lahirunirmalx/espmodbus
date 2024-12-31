
# NRF24Communicator: ESP32 + NRF24L01 + OLED Display + PS/2 Keyboard

This project implements a wireless communicator using an **ESP32**, **NRF24L01 module**, **OLED display**, and a **PS/2 keyboard**. It supports sending and receiving messages wirelessly while displaying data in real-time on the OLED screen. The system includes interactive controls for navigating, composing, and reading messages.

---

## Features
- **Wireless Communication**: Send and receive messages using the NRF24L01 module.
- **OLED Display**: Displays real-time data, message inbox, and menu navigation.
- **PS/2 Keyboard Input**: Allows text input and navigation through messages.
- **Interactive Menu**: Navigate through different modes such as reading messages, composing new ones, and setting addresses.
- **Customizable Address**: Dynamically configure communication addresses.
- **Boot Button Actions**: 
  - Short press: Send a predefined message.
  - Long press: Access the first message in the inbox.

---

## Requirements
- **Hardware:**
  - ESP32 Microcontroller
  - NRF24L01 Wireless Module
  - OLED Display (e.g., SSD1306)
  - PS/2 Keyboard
  - OneButton (for boot actions)
  - Jumper wires and breadboard (or PCB)
  
- **Libraries:**
  Install these libraries using the Arduino Library Manager:
  - `RF24` (for NRF24L01 communication)
  - `Adafruit_SSD1306` and `Adafruit_GFX` (for OLED display)
  - `OneButton` (for button management)
  - `PS2Keyboard` (for PS/2 keyboard input)

---

## Pinout

### NRF24L01 Module to ESP32
| NRF24L01 Pin | ESP32 Pin   | Description                |
|--------------|-------------|----------------------------|
| VCC          | 3.3V        | Power supply (3.3V only)   |
| GND          | GND         | Ground                     |
| CE           | GPIO 4      | Chip Enable                |
| CSN          | GPIO 5      | Chip Select Not            |
| SCK          | GPIO 18     | SPI Clock                  |
| MOSI         | GPIO 23     | Master Out Slave In        |
| MISO         | GPIO 19     | Master In Slave Out        |

### OLED Display to ESP32
| OLED Pin     | ESP32 Pin   | Description                |
|--------------|-------------|----------------------------|
| VCC          | 3.3V        | Power supply               |
| GND          | GND         | Ground                     |
| SCL          | GPIO 22     | I2C Clock                  |
| SDA          | GPIO 21     | I2C Data                   |

### PS/2 Keyboard to ESP32
| PS/2 Pin     | ESP32 Pin   | Description                |
|--------------|-------------|----------------------------|
| Data         | GPIO 32     | Data Line                  |
| Clock        | GPIO 33     | Clock Line                 |
| VCC          | 5V/3.3V     | Power Supply               |
| GND          | GND         | Ground                     |

---

## Functional Modes
1. **Home**: Displays a list of messages in the inbox.
2. **New Message**: Compose a new message.
3. **Read Message**: View the selected message.
4. **Set Address**: Change the communication address dynamically.
5. **Send Message**: Send a composed message to the selected address.
6. **Delete Message**: Delete a selected message from the inbox.
7. **Error**: Displays error states, such as failed message transmission.

---

## Usage
1. **Setup**:
   - Wire the NRF24L01, OLED, and PS/2 keyboard to the ESP32 using the pinout table.
   - Install the required libraries in your Arduino IDE.

2. **Running the Code**:
   - Clone the repository and open the `.ino` file in Arduino IDE or PlatformIO:
     ```bash
     git clone https://github.com/lahirunirmalx/NRF24Communicator.git
     ```
   - Configure the `COMMUNICATION_ADDRESS` in the code as required.
   - Upload the sketch to your ESP32.

3. **Navigation**:
   - Use the PS/2 keyboard to navigate and interact with the system.
   - Use the boot button for quick actions:
     - Short press: Sends a predefined message.
     - Long press: Opens the first message in the inbox.

---

## Example Output

**Home Screen:**
```
[Node] 00001 [ch] 1
[=========]   05
[00002] Hello Wo *
[00003] Hi there
```

**Read Message:**
```
[Node] 00001 [ch] 1
[=========]   03
From: 00003
Hello, this is a test message!
```

**Write Message:**
```
[Node] 00001 [ch] 1
[=========]   00
To: 00003
Message: Test Message
```

---

## License
This project is open source under the MIT License.

---

## Contributions
Contributions are welcome! Feel free to fork this repository, submit issues, or create pull requests for improvements and new features.

 