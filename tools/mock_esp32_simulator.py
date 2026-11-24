#!/usr/bin/env python3
"""
============================================================================
TRAZEN: MOCK ESP32-CAM & GPS VEHICULAR SIMULATOR
Runs on your laptop to simulate the ESP32-CAM edge node without physical hardware.
Streams synthetic road frames, computes HMAC-SHA256, and simulates GPS telemetry.
============================================================================
"""

import time
import math
import hmac
import hashlib
import urllib.request
import urllib.error
from datetime import datetime

# Server Configuration
SERVER_URL = "http://127.0.0.1:5000/api/v1/frames"
SECRET_KEY = "trazen-secret-key-licet-2026"
DEVICE_ID = "TRAZEN-NODE-01"

# Base GPS Center: Loyola ICAM College of Engineering and Technology (LICET), Chennai
BASE_LAT = 13.06042
BASE_LON = 80.24965

def create_synthetic_jpeg(width=640, height=480, frame_num=1, speed=0.0):
    """
    Generates a minimal valid JPEG payload in pure standard Python (no external dependencies required).
    """
    # A standard 1x1 minimal valid JPEG header and EOF template
    # For simulation, we create a valid baseline JPEG stream
    header = bytes.fromhex(
        "ffd8ffe000104a46494600010101004800480000ffdb00430008060607060508070707090908"
        "0a0c140d0c0b0b0c1912130f141d1a1f1e1d1a1c1c20242e2720222c231c1c2837292c303134"
        "34341f27393d38323c2e333432ffc0000b080001000101011100ffc4001f0000010501010101"
        "010100000000000000000102030405060708090a0bffda0008010100003f007f00ffd9"
    )
    return header

def generate_hmac_sha256(payload_bytes, key_str):
    return hmac.new(key_str.encode("utf-8"), payload_bytes, hashlib.sha256).hexdigest()

def run_simulation(duration_seconds=60, interval=1.5):
    print(f"\n========================================================")
    print(f"  TRAZEN EDGE SIMULATOR ACTIVE")
    print(f"  Simulating: {DEVICE_ID} transmitting to {SERVER_URL}")
    print(f"  Coordinates: Loyola ICAM Campus, Chennai")
    print(f"  Press Ctrl+C to stop simulation.")
    print(f"========================================================\n")

    start_time = time.time()
    frame_id = 0
    angle = 0.0

    while time.time() - start_time < duration_seconds:
        frame_id += 1
        angle += 0.08

        # Simulate vehicle cruising around campus circle (radius ~300 meters)
        lat = BASE_LAT + (0.0025 * math.sin(angle))
        lon = BASE_LON + (0.0025 * math.cos(angle))

        # Simulate speed: mostly 32-38 km/h, with intentional spike every 8 frames to trigger violation!
        if frame_id % 8 == 0:
            speed = 54.8 # Speeding violation!
            simulated_signal = "RED" # Also red light violation!
            print(f"\n🚨 [SIMULATOR] Triggering intentional road infraction (Speed: {speed} km/h | Signal: RED)")
        else:
            speed = 34.0 + (3.0 * math.sin(angle * 2))
            simulated_signal = "GREEN"

        # Generate JPEG frame
        jpeg_bytes = create_synthetic_jpeg(640, 480, frame_id, speed)

        # Generate HMAC-SHA256 signature
        signature = generate_hmac_sha256(jpeg_bytes, SECRET_KEY)

        # Build HTTP Request
        headers = {
            "Content-Type": "image/jpeg",
            "X-Device-Id": DEVICE_ID,
            "X-Frame-Id": str(frame_id),
            "X-Timestamp-Ms": str(int(time.time() * 1000)),
            "X-Signature-SHA256": signature,
            "X-GPS-Valid": "true",
            "X-GPS-Latitude": f"{lat:.6f}",
            "X-GPS-Longitude": f"{lon:.6f}",
            "X-GPS-Speed": f"{speed:.2f}",
            "X-GPS-Heading": f"{math.degrees(angle) % 360:.1f}",
            "X-Traffic-Signal": simulated_signal
        }

        req = urllib.request.Request(SERVER_URL, data=jpeg_bytes, headers=headers, method="POST")

        try:
            with urllib.request.urlopen(req, timeout=3) as resp:
                status_code = resp.getcode()
                response_text = resp.read().decode("utf-8")
                print(f"[NODE-01] Frame #{frame_id:03d} -> Server 201 Created | Speed: {speed:.1f} km/h | Lat: {lat:.5f}, Lon: {lon:.5f}")
        except urllib.error.URLError as e:
            print(f"[NODE-01] Connection Error: {e}. Is the Flask server running on port 5000?")

        time.sleep(interval)

if __name__ == "__main__":
    try:
        run_simulation(duration_seconds=300, interval=1.5)
    except KeyboardInterrupt:
        print("\n[SIMULATOR] Stopped by user.")
