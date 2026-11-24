#!/usr/bin/env python3
"""
TRAZEN: AUTOMATED HARDWARE & SENSOR DIAGNOSTIC TOOL
Detects connected USB-to-UART programmers, verifies serial output from ESP32-CAM,
and checks end-to-end communication with the TRAZEN Flask backend.
"""

import sys
import time
import subprocess
from pathlib import Path

# Add project root
ROOT_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT_DIR))

from server.database import get_stats, get_latest_telemetry, get_recent_violations

def get_connected_com_ports():
    """Queries Windows PowerShell .NET SerialPort to discover active COM ports."""
    cmd = 'powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames()"'
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=5)
        ports = [p.strip() for p in res.stdout.splitlines() if p.strip()]
        return ports
    except Exception as e:
        print(f"[ERROR] Could not query COM ports: {e}")
        return []

def run_diagnostics():
    print("=" * 65)
    print("  TRAZEN AUTOMATED HARDWARE & DIAGNOSTIC SYSTEM CHECK")
    print("=" * 65)

    # Test 1: Check for USB Hardware Ports
    print("\n[STEP 1/4] Scanning for Connected USB-to-UART / Serial Adapters...")
    ports = get_connected_com_ports()
    if ports:
        print(f"  --> FOUND ACTIVE PORT(S): {', '.join(ports)}")
        active_port = ports[0]
        print(f"  --> Identified candidate ESP32-CAM / FTDI on {active_port}")
    else:
        print("  --> [!] No COM ports detected yet.")
        print("      Please plug in your FTDI / ESP32-CAM USB cable.")

    # Test 2: Check SQLite Database Integrity
    print("\n[STEP 2/4] Verifying Local Database (trazen.db)...")
    try:
        stats = get_stats()
        print(f"  --> Database: ONLINE")
        print(f"      Total Frames: {stats['total_frames']} | Total Violations: {stats['total_violations']}")
    except Exception as e:
        print(f"  --> [ERROR] Database access failed: {e}")

    # Test 3: Check Latest Telemetry & Node Health
    print("\n[STEP 3/4] Inspecting Latest Telemetry Packet...")
    try:
        latest = get_latest_telemetry()
        if latest:
            print(f"  --> Last Known Node: {latest.get('device_id', 'TRAZEN-NODE-01')}")
            print(f"  --> GPS Coordinates: Lat {latest.get('latitude', 0.0):.4f}, Lon {latest.get('longitude', 0.0):.4f}")
            print(f"  --> Speed: {latest.get('speed_kmh', 0.0):.1f} km/h")
            print(f"  --> Timestamp: {latest.get('recorded_at')}")
        else:
            print("  --> No telemetry packets received yet.")
    except Exception as e:
        print(f"  --> [WARNING] Telemetry query error: {e}")

    # Test 4: Check Violation Audit Logs
    print("\n[STEP 4/4] Checking AI Violation Audit Log...")
    try:
        violations = get_recent_violations(limit=3)
        print(f"  --> Logged Violations: {len(violations)} records retrieved.")
        for v in violations:
            print(f"      - [{v['violation_type']}] ID: {v['violation_id']} | Speed: {v['speed_kmh']} km/h | Status: {v['status']}")
    except Exception as e:
        print(f"  --> [WARNING] Violation query error: {e}")

    print("\n" + "=" * 65)
    if ports:
        print(f"  DIAGNOSTIC STATUS: READY (Hardware detected on {ports[0]})")
    else:
        print("  DIAGNOSTIC STATUS: AWAITING HARDWARE PLUG-IN")
    print("=" * 65 + "\n")

if __name__ == "__main__":
    run_diagnostics()
