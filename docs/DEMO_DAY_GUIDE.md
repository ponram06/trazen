# TRAZEN Demo-Day Guide

This project is a **prototype evidence recorder**: the ESP32-CAM sends JPEG
frames, the NEO-6M contributes verified GPS speed and location, and the laptop
dashboard records signed frames.  Present red-light and wrong-way detection as
future computer-vision work unless you are using the supplied simulator.

## 1. Prepare once, before the demo

1. Copy `firmware/esp32_cam/config.h` to `firmware/esp32_cam/config.local.h`.
   Set the hotspot Wi-Fi name/password, laptop IPv4 address, and a long random
   `HMAC_SECRET_KEY`. Never commit this file.
2. On the laptop, install dependencies with `python -m pip install -r requirements.txt`.
3. Put the laptop and ESP32-CAM on the same mobile hotspot. Find the laptop's
   Wi-Fi IPv4 address with `ipconfig`; use that address as `SERVER_HOST`.
4. Allow Python through Windows Firewall when Windows asks. Keep port 5000 open
   for a Private network.
5. Flash the ESP32-CAM, remove the GPIO 0-to-GND jumper, then reset it.
6. Test outdoors or beside a clear window for 2–5 minutes. The NEO-6M needs a
   satellite fix; do not expect a reliable indoor fix.

## 2. Start sequence on demo day

In PowerShell at the project root, choose a secret exactly matching
`HMAC_SECRET_KEY` in `config.local.h`, then run:

```powershell
$env:TRAZEN_SECRET_KEY = "use-the-same-secret-as-config-local-h"
python -m server.app
```

Open `http://localhost:5000`. In a second PowerShell window run:

```powershell
$env:TRAZEN_SECRET_KEY = "use-the-same-secret-as-config-local-h"
python tools/hardware_diagnostic.py
```

Power the ESP32-CAM from a stable 5 V supply. The FTDI adapter should provide
serial data and common ground; it is not a dependable camera power supply.

## 3. What to show (three minutes)

1. Show the board, GPS wiring, and Serial Monitor: Wi-Fi connected, camera
   initialized, then HTTP status `201`.
2. Show the browser dashboard: a fresh road frame, speed, live GPS point, and
   the growing frame counter.
3. Explain the HMAC result: a frame whose signature is missing or changed is
   rejected by the server. This is integrity checking, not a government-issued
   legal challan.
4. If moving vehicle testing is unsafe, show the physical camera/GPS feed then
   run the simulator as the marked backup demonstration.

## 4. Simulator backup

With the server still running and the same environment variable set:

```powershell
python tools/mock_esp32_simulator.py
```

It sends signed synthetic frames and intentional speed-rule events. State
clearly that it is a simulator used to demonstrate the dashboard/event flow.

## 5. Fast troubleshooting

* `401 Invalid or missing frame signature`: the firmware and PowerShell secret
  differ.
* No frame / connection error: confirm both devices use the same hotspot and
  re-check `SERVER_HOST` with `ipconfig`.
* Brownout/camera failure: use a stronger 5 V supply and connect every ground.
* `GPS ACQUIRING`: take the antenna outdoors and wait for a fix.
