# TRAZEN: Smart Traffic Violation Detection System

[![Python](https://img.shields.io/badge/Python-3.10%2B-blue.svg)](https://www.python.org/)
[![Flask](https://img.shields.io/badge/Flask-3.1-black.svg)](https://flask.palletsprojects.com/)
[![ESP32-CAM](https://img.shields.io/badge/ESP32--CAM-AI--Thinker-red.svg)](https://www.espressif.com/)
[![GPS](https://img.shields.io/badge/GPS-NEO--6M-orange.svg)](https://www.u-blox.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

An intelligent, edge-assisted vehicular traffic enforcement and telemetry system developed as an engineering capstone project.

* **Author:** Jebinson Raj. R
* **Department:** Electronics and Communication Engineering (ECE)
* **Institution:** Loyola ICAM College of Engineering and Technology (LICET), Chennai
* **Academic Year:** 2026–2027

---

## 🚦 Overview

**TRAZEN** addresses the spatial blind spots and high infrastructure costs of traditional static traffic cameras by deploying a mobile, vehicle-mounted IoT edge node. Combining an **AI-Thinker ESP32-CAM** and a **u-blox NEO-6M GPS receiver**, TRAZEN captures real-time road feeds synchronized with GPS speed, coordinates, and UTC timestamps.

Infractions such as **overspeeding**, **red light signal jumping**, and **wrong-way driving** are evaluated in real time. Detected violations generate an immutable evidence package authenticated via **HMAC-SHA256** and cataloged on an interactive Leaflet.js command dashboard capable of producing official printable E-Challans.

---

## 📐 System Architecture

```
[VEHICLE EDGE SENSORS]
  ESP32-CAM (OV2640) + NEO-6M GPS Module
          │
          │ (Wi-Fi / Hotspot - HTTP POST + HMAC-SHA256)
          ▼
[IN-CABIN GATEWAY / FLASK SERVER]
  ├── Cryptographic Integrity Engine (HMAC Verification)
  ├── AI Heuristics Engine (Speed, Red Light, Wrong-Way)
  └── SQLite Relational Database (trazen.db)
          │
          │ (REST API & Real-Time Polling)
          ▼
[TRAZEN WEB COMMAND CENTER]
  ├── Live Video HUD with Speedometer
  ├── Interactive Leaflet.js GPS Route Tracker
  ├── Real-Time Violation Audit Feed
  └── Legal-Grade E-Challan & Evidence Generator
```

---

## ⚡ Key Features

* **Dual-Tier Edge/Fog Computing:** Low-power sensor acquisition and hardware cryptographic signing on the ESP32 node; heavy heuristic inference and presentation on the server.
* **Tamper-Evident Chain-of-Custody:** Raw frames are hashed using HMAC-SHA256 on the microcontroller, producing verifiable cryptographic provenance admissible for traffic citations.
* **Real-Time Interactive GPS Mapping:** Tracks vehicle breadcrumb trails and flags incident pins using Leaflet.js and OpenStreetMap.
* **Official E-Challan Generation:** One-click generation of printable citations formatted under the Motor Vehicles regulatory standards.
* **Zero-Hardware Desktop Simulator:** Includes a built-in mock client to simulate road feeds and GPS coordinates for presentations without physical hardware.

---

## 🔌 Hardware Pinout Connections

| Component Pin | ESP32-CAM Pin | Purpose / Function |
| :--- | :--- | :--- |
| **NEO-6M GPS TX** | **GPIO 13** | UART2 RX (NMEA Telemetry stream) |
| **NEO-6M GPS RX** | **GPIO 14** | UART2 TX |
| **NEO-6M GPS VCC** | **5V** | Regulated Power |
| **NEO-6M GPS GND** | **GND** | Common Ground |
| **FTDI TXD** | **U0R (GPIO 3)** | Serial Upload (Programming) |
| **FTDI RXD** | **U0T (GPIO 1)** | Serial Monitor |
| **Jumper Wire** | **GPIO 0 to GND** | **Flashing Mode Only** (Remove to Run) |

*For complete wiring schematics, see [`docs/WIRING_GUIDE.md`](docs/WIRING_GUIDE.md).*

---

## 🚀 Quick Start Guide

### 1. Clone the Repository
```bash
git clone https://github.com/ponram06/trazen.git
cd trazen
```

### 2. Install Dependencies
```bash
pip install -r requirements.txt
```

### 3. Seed Demo Violations (Optional)
```bash
python tools/seed_demo_data.py
```

### 4. Start the TRAZEN Server
```bash
python -m server.app
```
*Open [http://localhost:5000](http://localhost:5000) in your web browser.*

### 5. Launch the Desktop Edge Simulator
To simulate live vehicle GPS movement and automatic violation triggers:
```bash
python tools/mock_esp32_simulator.py
```

---

## 📂 Repository Structure

```
trazen/
├── firmware/esp32_cam/       # Arduino C++ Firmware (Camera, GPS NMEA, HMAC-SHA256)
├── server/                   # Flask REST API, SQLite database, and Security layers
│   ├── templates/            # Dashboard, Violations table, and E-Challan HTML
│   └── static/               # CSS styles, Leaflet map engine, and JavaScript
├── ai_engine/                # Traffic violation heuristics & decision rules
├── tools/                    # Mock simulator and sample data generators
├── docs/                     # Wiring schematics and 5-minute presentation guide
├── requirements.txt          # Python dependencies
└── README.md                 # Project landing documentation
```

---

## 🎓 Academic Attribution

This project is submitted in partial fulfillment of the requirements for the degree of Bachelor of Engineering in **Electronics and Communication Engineering (ECE)** at **Loyola ICAM College of Engineering and Technology (LICET)**, Chennai.
