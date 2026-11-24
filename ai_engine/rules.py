# ============================================================================
# TRAZEN VIOLATION HEURISTIC RULES ENGINE
# Defines deterministic criteria for flagging road safety infractions.
# ============================================================================

def evaluate_speed_infraction(current_speed_kmh, speed_limit_kmh=40.0, tolerance_kmh=5.0):
    """
    Evaluates whether the vehicle's speed constitutes an infraction.
    Returns: dict with is_violation, excess_speed, severity, confidence.
    """
    if current_speed_kmh is None or current_speed_kmh <= 0:
        return {"is_violation": False}

    threshold = speed_limit_kmh + tolerance_kmh
    if current_speed_kmh > threshold:
        excess = current_speed_kmh - speed_limit_kmh
        if excess > 25.0:
            severity = "HIGH"
            confidence = 0.98
        elif excess > 10.0:
            severity = "MEDIUM"
            confidence = 0.95
        else:
            severity = "LOW"
            confidence = 0.88

        return {
            "is_violation": True,
            "violation_type": "OVERSPEEDING",
            "severity": severity,
            "confidence": confidence,
            "current_speed": current_speed_kmh,
            "speed_limit": speed_limit_kmh,
            "excess_speed": round(excess, 1),
            "remarks": f"Vehicle clocked at {current_speed_kmh:.1f} km/h in a {speed_limit_kmh:.0f} km/h zone (+{excess:.1f} km/h over limit)."
        }

    return {"is_violation": False}

def evaluate_traffic_signal_infraction(signal_color, vehicle_speed_kmh, crossed_stopline=True):
    """
    Evaluates if a red light jumping infraction occurred.
    Rule: Signal is RED, vehicle crossed stop line, and speed > 5 km/h.
    """
    if signal_color and signal_color.upper() == "RED" and crossed_stopline and vehicle_speed_kmh > 5.0:
        return {
            "is_violation": True,
            "violation_type": "RED_LIGHT_JUMP",
            "severity": "HIGH",
            "confidence": 0.96,
            "remarks": f"Vehicle crossed active stop line during steady RED phase at {vehicle_speed_kmh:.1f} km/h."
        }
    return {"is_violation": False}

def evaluate_wrong_way_infraction(vehicle_heading_deg, road_heading_deg, speed_kmh):
    """
    Evaluates if the vehicle is driving against designated traffic flow.
    Rule: Angle difference between heading and lane vector > 120 degrees at speed > 10 km/h.
    """
    if speed_kmh < 10.0:
        return {"is_violation": False}

    diff = abs(vehicle_heading_deg - road_heading_deg) % 360
    if diff > 180:
        diff = 360 - diff

    if diff > 120.0:
        return {
            "is_violation": True,
            "violation_type": "WRONG_WAY_DRIVING",
            "severity": "CRITICAL",
            "confidence": 0.94,
            "remarks": f"Direction vector deviated by {diff:.1f}° from designated lane flow."
        }
    return {"is_violation": False}
