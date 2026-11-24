# TRAZEN ESP32 Edge Node — Wokwi Online Simulation Guide

This guide details how to simulate the **TRAZEN Onboard Vehicular Edge Node** (ESP32-CAM + NEO-6M GPS) using the free online electronics simulator [Wokwi](https://wokwi.com/projects/new/esp32).

---

## 1. Quick Start in 30 Seconds

1. **Open Wokwi ESP32 Project**:
   A new ESP32 simulator tab has been opened in your browser at:
   **[https://wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32)**

2. **Paste Code (`sketch.ino`)**:
   - The simulation code has already been copied directly to your clipboard!
   - In the Wokwi tab, click inside the `sketch.ino` editor, press `Ctrl + A` (select all), and press `Ctrl + V` (paste).
   - Alternatively, copy the contents of [`wokwi_sketch.ino`](./wokwi_sketch.ino).

3. **(Optional) Diagram Layout (`diagram.json`)**:
   - Click the `diagram.json` tab in Wokwi and paste the contents of [`wokwi_diagram.json`](./wokwi_diagram.json) to wire the on-board Status/Capture LED and resistor.

4. **Start Simulation**:
   - Click the green **Play (▶ Start Simulation)** button at the top of Wokwi.
   - Watch the compilation in the browser and monitor real-time output in the **Serial Monitor** at the bottom!

---

## 2. What the Simulation Demonstrates

| Feature | Physical Edge Node | Wokwi Simulation Implementation |
| :--- | :--- | :--- |
| **Microcontroller** | AI-Thinker ESP32-CAM | ESP32 DevKit V4 Dual-Core Xtensa LX6 |
| **Camera Sensor** | OV2640 2-Megapixel DVP Sensor | Synthetic JPEG frame generation with dynamic payloads & hardware fallback |
| **GPS Telemetry** | u-blox NEO-6M UART GPS Module | Ingests real-time NMEA `$GPRMC` sentences simulating vehicular coordinates around LICET Campus, Chennai |
| **Cryptography** | Hardware HMAC-SHA256 frame signature | Real `mbedtls` cryptographic engine calculates SHA-256 HMAC digest |
| **Connectivity** | 802.11 b/g/n Wi-Fi Hotspot | Virtual connection to `Wokwi-GUEST` access point with local IP assignment |
| **Local Dashboard** | Embedded WebServer on port 80 | Full HTML/CSS telemetry status dashboard accessible via simulated web server |
| **Violation Alert** | Telemetry thresholds | Evaluates speed spikes (>36 km/h) and outputs edge warning markers |

---

## 3. Expected Serial Monitor Output

```text
========================================================
  TRAZEN: SMART TRAFFIC VIOLATION EDGE SIMULATION
  Dept of ECE, LICET Chennai | Online Wokwi Run
========================================================
[CAMERA] Physical sensor not present. Running in Wokwi Synthetic Mode.
[WIFI] Connecting to Wokwi Access Point: Wokwi-GUEST
...
[WIFI] Connected Successfully to Virtual Network!
[WIFI] Edge Node IP Address: 10.0.1.2
[WIFI] Gateway: 10.0.1.1
[SERVER] Local Web Monitor active on http://10.0.1.2/

[EDGE NODE] Frame #001 | Size: 134B | SHA256: 4b29c15e8f0a3d21...
            GPS: Lat 13.06042, Lon 80.24965 | Speed: 32.0 km/h | Status: NORMAL CRUISE

[EDGE NODE] Frame #002 | Size: 134B | SHA256: a718f3d09e32bb82...
            GPS: Lat 13.06198, Lon 80.25121 | Speed: 38.2 km/h | Status: 🚨 SPEED ALERT!
```
