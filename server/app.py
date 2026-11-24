import os
import time
from datetime import datetime
from flask import Flask, render_template, request, jsonify, send_from_directory, redirect, url_for

from .config import (
    BASE_DIR, STATIC_DIR, FRAMES_DIR, VIOLATIONS_DIR,
    HOST, PORT, DEBUG, SECRET_KEY, DEFAULT_DEVICE_ID
)
from .database import (
    init_db, log_frame, log_telemetry, get_recent_violations,
    get_violation_by_id, update_violation_status, get_stats,
    get_latest_telemetry, get_telemetry_history
)
from .security import verify_hmac_sha256, calculate_sha256
from ai_engine.detector import TrafficViolationDetector

# Initialize Flask App
app = Flask(
    __name__,
    static_folder=str(STATIC_DIR),
    template_folder=str(BASE_DIR / "templates")
)
app.secret_key = SECRET_KEY

# Initialize Database & AI Engine
init_db()
detector = TrafficViolationDetector()

# Global in-memory cache of latest frame filename for instant live-stream display
latest_frame_info = {
    "filename": None,
    "timestamp": None,
    "device_id": DEFAULT_DEVICE_ID,
    "speed": 0.0,
    "lat": 13.0604,
    "lon": 80.2496
}

# ============================================================================
# WEB DASHBOARD ROUTES (HTML PAGES)
# ============================================================================

@app.route("/")
def index():
    """Main Operational Live Dashboard."""
    stats = get_stats()
    violations = get_recent_violations(limit=10)
    latest_tel = get_latest_telemetry() or {
        "latitude": 13.0604, "longitude": 80.2496, "speed_kmh": 0.0, "recorded_at": "Just now"
    }
    return render_template(
        "index.html",
        stats=stats,
        violations=violations,
        telemetry=latest_tel,
        latest_frame=latest_frame_info
    )

@app.route("/violations")
def violations_page():
    """Violations Log & Administrative Audit Page."""
    v_type = request.args.get("type")
    status = request.args.get("status")
    violations = get_recent_violations(limit=100, violation_type=v_type, status=status)
    return render_template("violations.html", violations=violations, current_type=v_type, current_status=status)

@app.route("/challan/<violation_id>")
def challan_page(violation_id):
    """Printable Traffic E-Challan / Evidence Document."""
    violation = get_violation_by_id(violation_id)
    if not violation:
        return "Violation record not found", 404
    return render_template("challan.html", violation=violation)

# ============================================================================
# REST API ENDPOINTS
# ============================================================================

@app.route("/api/v1/frames", methods=["POST"])
@app.route("/api/v1/events/<event_id>/frames", methods=["POST"])
@app.route("/upload", methods=["POST"])
def ingest_frame(event_id=None):
    """
    Primary ingestion endpoint for ESP32-CAM and Simulator frames.
    Receives JPEG image payload with cryptographic headers and telemetry metadata.
    """
    # Support both raw binary payload and multipart/form-data uploads
    if request.files:
        uploaded_file = next(iter(request.files.values()))
        raw_data = uploaded_file.read()
    else:
        raw_data = request.get_data()

    if not raw_data:
        return jsonify({"status": "error", "message": "Empty payload"}), 400

    # Extract headers transmitted by ESP32-CAM
    device_id = request.headers.get("X-Device-Id", DEFAULT_DEVICE_ID)
    frame_id = request.headers.get("X-Frame-Id", "0")
    signature = request.headers.get("X-Signature-SHA256", "")
    lat = float(request.headers.get("X-GPS-Latitude", 13.0604))
    lon = float(request.headers.get("X-GPS-Longitude", 80.2496))
    speed = float(request.headers.get("X-GPS-Speed", 0.0))
    heading = float(request.headers.get("X-GPS-Heading", 0.0))
    is_gps_valid = request.headers.get("X-GPS-Valid", "true").lower() == "true"
    simulated_signal = request.headers.get("X-Traffic-Signal", None)

    # Verify HMAC-SHA256 signature
    is_signature_valid = True
    if signature:
        is_signature_valid = verify_hmac_sha256(raw_data, signature, SECRET_KEY)
        if not is_signature_valid:
            print(f"[SECURITY ALERT] Invalid signature rejected for device {device_id}!")
            # Still proceed or reject based on security mode; we log signature_verified=0
    
    # Save frame file to disk
    timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    filename = f"frame_{device_id}_{timestamp_str}.jpg"
    filepath = FRAMES_DIR / filename

    with open(filepath, "wb") as f:
        f.write(raw_data)

    # Update in-memory live stream tracker
    global latest_frame_info
    latest_frame_info = {
        "filename": filename,
        "url": f"/static/uploads/frames/{filename}",
        "timestamp": datetime.now().strftime("%H:%M:%S"),
        "device_id": device_id,
        "speed": speed,
        "lat": lat,
        "lon": lon
    }

    # Record frame in SQLite
    log_frame(
        device_id=device_id,
        frame_id=int(frame_id) if frame_id.isdigit() else 0,
        filename=filename,
        size_bytes=len(raw_data),
        latitude=lat,
        longitude=lon,
        speed_kmh=speed,
        is_gps_valid=is_gps_valid,
        signature_verified=1 if is_signature_valid else 0
    )

    # Record GPS Telemetry trail
    log_telemetry(device_id, lat, lon, speed, heading, is_gps_valid)

    # Run AI Violation Detection Engine on the frame
    detected_violations = detector.process_frame(
        frame_path=filepath,
        raw_bytes=raw_data,
        device_id=device_id,
        latitude=lat,
        longitude=lon,
        speed_kmh=speed,
        heading_deg=heading,
        simulated_signal=simulated_signal
    )

    return jsonify({
        "status": "success",
        "frame_id": frame_id,
        "signature_verified": is_signature_valid,
        "violations_detected": len(detected_violations),
        "violations": detected_violations
    }), 201

@app.route("/api/v1/telemetry", methods=["POST"])
def ingest_telemetry():
    """Endpoint for periodic GPS telemetry ping."""
    data = request.get_json(silent=True) or request.form
    device_id = data.get("device_id", DEFAULT_DEVICE_ID)
    lat = float(data.get("latitude", 13.0604))
    lon = float(data.get("longitude", 80.2496))
    speed = float(data.get("speed_kmh", 0.0))
    heading = float(data.get("heading_deg", 0.0))

    log_telemetry(device_id, lat, lon, speed, heading)
    return jsonify({"status": "success"}), 200

@app.route("/api/v1/telemetry/live", methods=["GET"])
def get_live_telemetry():
    """Returns current device coordinates and recent trail points for Leaflet.js map."""
    device_id = request.args.get("device_id", DEFAULT_DEVICE_ID)
    latest = get_latest_telemetry(device_id)
    history = get_telemetry_history(device_id, limit=30)
    return jsonify({
        "latest": latest,
        "trail": history,
        "latest_frame": latest_frame_info
    })

@app.route("/api/v1/violations", methods=["GET"])
def api_violations():
    """Returns list of recent violations."""
    v_type = request.args.get("type")
    status = request.args.get("status")
    limit = int(request.args.get("limit", 50))
    records = get_recent_violations(limit=limit, violation_type=v_type, status=status)
    return jsonify({"count": len(records), "violations": records})

@app.route("/api/v1/violations/<violation_id>/review", methods=["POST"])
def review_violation(violation_id):
    """Allows administrators to approve or dismiss a violation."""
    data = request.get_json(silent=True) or request.form
    new_status = data.get("status", "APPROVED")
    remarks = data.get("remarks")
    success = update_violation_status(violation_id, new_status, remarks)
    if success:
        return jsonify({"status": "success", "violation_id": violation_id, "new_status": new_status})
    return jsonify({"status": "error", "message": "Violation ID not found"}), 404

@app.route("/api/v1/stats", methods=["GET"])
def api_stats():
    """Returns real-time analytics for live counter badges."""
    return jsonify(get_stats())

# ============================================================================
# ENTRY POINT
# ============================================================================
if __name__ == "__main__":
    print(f"\n========================================================")
    print(f"  TRAZEN: SMART TRAFFIC VIOLATION DETECTION SERVER")
    print(f"  Active on http://localhost:{PORT}")
    print(f"  Department of ECE, LICET (2026-2027)")
    print(f"========================================================\n")
    app.run(host=HOST, port=PORT, debug=DEBUG)
