import os
import time
import uuid
import shutil
from pathlib import Path
from datetime import datetime

from .rules import (
    evaluate_speed_infraction,
    evaluate_traffic_signal_infraction,
    evaluate_wrong_way_infraction
)
from server.database import log_violation
from server.security import calculate_sha256
from server.config import VIOLATIONS_DIR, DEFAULT_SPEED_LIMIT_KMH

class TrafficViolationDetector:
    def __init__(self, speed_limit_kmh=DEFAULT_SPEED_LIMIT_KMH):
        self.speed_limit_kmh = speed_limit_kmh

    def process_frame(self, frame_path, raw_bytes, device_id, latitude, longitude, speed_kmh, heading_deg=0.0, simulated_signal=None):
        """
        Processes an incoming frame and its telemetry to determine if a violation occurred.
        Returns a violation dict if detected, or None.
        """
        violations_detected = []

        # 1. Evaluate Speed Violation
        speed_eval = evaluate_speed_infraction(speed_kmh, self.speed_limit_kmh)
        if speed_eval["is_violation"]:
            violations_detected.append(speed_eval)

        # 2. Evaluate Traffic Signal Violation (if signal status passed or detected)
        if simulated_signal:
            signal_eval = evaluate_traffic_signal_infraction(simulated_signal, speed_kmh)
            if signal_eval["is_violation"]:
                violations_detected.append(signal_eval)

        # 3. If any violation is flagged, package official evidence
        created_records = []
        for eval_result in violations_detected:
            timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
            unique_suffix = uuid.uuid4().hex[:6].upper()
            violation_id = f"VIO-{timestamp_str}-{unique_suffix}"

            evidence_filename = f"{violation_id}.jpg"
            evidence_full_path = VIOLATIONS_DIR / evidence_filename

            # Save evidence frame
            with open(evidence_full_path, "wb") as f:
                f.write(raw_bytes)

            # Compute tamper-evident SHA-256 checksum
            evidence_hash = calculate_sha256(raw_bytes)

            # Record in SQLite database
            log_violation(
                violation_id=violation_id,
                device_id=device_id,
                violation_type=eval_result["violation_type"],
                severity=eval_result.get("severity", "MEDIUM"),
                confidence=eval_result.get("confidence", 0.95),
                speed_kmh=speed_kmh,
                speed_limit_kmh=self.speed_limit_kmh,
                latitude=latitude,
                longitude=longitude,
                evidence_image=f"uploads/violations/{evidence_filename}",
                evidence_hash_sha256=evidence_hash,
                remarks=eval_result.get("remarks")
            )

            created_records.append({
                "violation_id": violation_id,
                "violation_type": eval_result["violation_type"],
                "severity": eval_result.get("severity", "MEDIUM"),
                "speed_kmh": speed_kmh,
                "latitude": latitude,
                "longitude": longitude,
                "evidence_image": f"/static/uploads/violations/{evidence_filename}",
                "evidence_hash": evidence_hash,
                "remarks": eval_result.get("remarks")
            })

        return created_records
