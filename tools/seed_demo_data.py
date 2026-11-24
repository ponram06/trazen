#!/usr/bin/env python3
"""
Seeds realistic demonstration violations into the TRAZEN database.
Creates certified sample evidence files with verified SHA-256 signatures.
"""

import os
import sys
from pathlib import Path

# Add project root to path
ROOT_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT_DIR))

from server.config import VIOLATIONS_DIR, DB_PATH
from server.database import init_db, log_violation
from server.security import calculate_sha256
from tools.mock_esp32_simulator import create_synthetic_jpeg

def seed_violations():
    print("[SEED] Initializing database...")
    init_db()

    demo_records = [
        {
            "id": "VIO-20261124-091422-SP10",
            "type": "OVERSPEEDING",
            "severity": "HIGH",
            "confidence": 0.98,
            "speed": 62.4,
            "limit": 40.0,
            "lat": 13.06112,
            "lon": 80.24890,
            "remarks": "Vehicle clocked at 62.4 km/h in a 40 km/h zone (+22.4 km/h over limit) along Sterling Road junction."
        },
        {
            "id": "VIO-20261124-110530-RL44",
            "type": "RED_LIGHT_JUMP",
            "severity": "HIGH",
            "confidence": 0.96,
            "speed": 34.2,
            "limit": 40.0,
            "lat": 13.05980,
            "lon": 80.25110,
            "remarks": "Vehicle crossed active pedestrian stop line during steady RED phase at LICET Main Gate signal."
        },
        {
            "id": "VIO-20261124-142210-WW09",
            "type": "WRONG_WAY_DRIVING",
            "severity": "CRITICAL",
            "confidence": 0.93,
            "speed": 28.5,
            "limit": 40.0,
            "lat": 13.06200,
            "lon": 80.24750,
            "remarks": "Vehicle direction vector deviated by 168.4° from designated one-way corridor flow."
        }
    ]

    for item in demo_records:
        evidence_filename = f"{item['id']}.jpg"
        evidence_path = VIOLATIONS_DIR / evidence_filename

        # Write sample valid JPEG evidence
        jpeg_data = create_synthetic_jpeg(640, 480, 1, item["speed"])
        with open(evidence_path, "wb") as f:
            f.write(jpeg_data)

        # Compute tamper-proof SHA-256
        sha256_hash = calculate_sha256(jpeg_data)

        log_violation(
            violation_id=item["id"],
            device_id="TRAZEN-NODE-01",
            violation_type=item["type"],
            severity=item["severity"],
            confidence=item["confidence"],
            speed_kmh=item["speed"],
            speed_limit_kmh=item["limit"],
            latitude=item["lat"],
            longitude=item["lon"],
            evidence_image=f"uploads/violations/{evidence_filename}",
            evidence_hash_sha256=sha256_hash,
            remarks=item["remarks"]
        )
        print(f"  + Seeded {item['id']} ({item['type']})")

    print("[SEED] Successfully seeded demonstration violations into SQLite!")

if __name__ == "__main__":
    seed_violations()
