# TRAZEN: Hardware Wiring & Pinout Guide

This guide details the exact electrical connections between the **AI-Thinker ESP32-CAM**, the **u-blox NEO-6M GPS Module**, and the **FTDI USB-to-UART Programmer**.

---

## 1. Components Checklist
1. **AI-Thinker ESP32-CAM** (with OV2640 camera module inserted)
2. **NEO-6M GPS Module** (with external patch antenna)
3. **FTDI 232 USB-to-TTL Serial Programmer** (or Arduino UNO used as USB-to-Serial bridge)
4. Jumper wires (female-to-female and female-to-male)
5. Breadboard
6. Regulated 5V DC Power Source (at least 2A; a phone charger or 5V 2A USB power bank works best)

---

## 2. Wiring for Flashing / Programming (ESP32-CAM to FTDI)

> [!IMPORTANT]
> **Flashing Mode Requirement:** GPIO 0 MUST be connected to GND before powering on the ESP32-CAM to put it into download/bootloader mode. After uploading finishes, disconnect GPIO 0 from GND and press the Reset button on the ESP32-CAM.

| FTDI Pin | ESP32-CAM Pin | Purpose |
| :--- | :--- | :--- |
| **VCC (Set to 5V)** | **5V** | Power supply input |
| **GND** | **GND** | Ground reference |
| **TXD** | **U0R (GPIO 3)** | Serial Receive on ESP32 |
| **RXD** | **U0T (GPIO 1)** | Serial Transmit on ESP32 |
| *Jumper Wire* | **GPIO 0 to GND** | **REQUIRED FOR PROGRAMMING** |

```
    FTDI Programmer              AI-Thinker ESP32-CAM
  +------------------+         +---------------------+
  | VCC (5V)         |-------->| 5V                  |
  | GND              |-------->| GND                 |
  | TXD              |-------->| U0R (GPIO 3)        |
  | RXD              |-------->| U0T (GPIO 1)        |
  +------------------+         |                     |
                               | GPIO 0 ----+        |
                               | GND   -----+ (BRIDGE)|
                               +---------------------+
```

---

## 3. Wiring for Normal Operation with NEO-6M GPS

After flashing the firmware, remove the GPIO 0 to GND jumper. Connect the NEO-6M GPS module:

| NEO-6M GPS Pin | ESP32-CAM Pin | Purpose / Notes |
| :--- | :--- | :--- |
| **VCC** | **5V** (or 3.3V) | Module power |
| **GND** | **GND** | Ground reference |
| **TX** | **GPIO 13** | ESP32 receives NMEA GPS data |
| **RX** | **GPIO 14** | ESP32 transmits configuration to GPS |

```
    NEO-6M GPS Module            AI-Thinker ESP32-CAM
  +------------------+         +---------------------+
  | VCC              |-------->| 5V                  |
  | GND              |-------->| GND                 |
  | TX               |-------->| GPIO 13 (Serial2 RX)|
  | RX               |-------->| GPIO 14 (Serial2 TX)|
  +------------------+         +---------------------+
```

---

## 4. Power Supply Tips (Preventing Brownout Resets)
* The ESP32-CAM consumes up to **300–450 mA during Wi-Fi transmission bursts**.
* Powering it solely from low-power laptop USB ports or cheap FTDI chips often causes **brownout errors** (`Brownout detector was triggered`).
* **Best practice:** Power the ESP32-CAM 5V and GND rails using an external 5V 2A adapter or power bank. Always ensure the FTDI ground, GPS ground, and power supply ground are tied together (Common Ground).

---

## 5. Arduino IDE Flashing Settings
1. Open **Arduino IDE**.
2. Go to **File -> Preferences** and add to Additional Board Manager URLs:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Go to **Tools -> Board -> ESP32 Arduino** and select **AI Thinker ESP32-CAM**.
4. Configure the following board settings:
   * **CPU Frequency:** 240MHz (WiFi/BT)
   * **Flash Frequency:** 80MHz
   * **Flash Mode:** QIO
   * **Partition Scheme:** Huge APP (3MB No OTA/1MB SPIFFS)
   * **Upload Speed:** 115200 (or 921600 if FTDI supports it)
5. In `config.h`, set your Wi-Fi SSID, Password, and Server IP.
6. Press the **Upload** button.
7. Once upload says *"Leaving... Hard resetting via RTS pin"*, unplug the GPIO 0 jumper and press the ESP32-CAM reset button!
