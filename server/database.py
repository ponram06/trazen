import sqlite3
import datetime
from .config import DB_PATH

def get_db_connection():
    """Returns a SQLite connection with dictionary-like row access."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    """Initializes all necessary database tables and indices."""
    with get_db_connection() as conn:
        cursor = conn.cursor()

        # Devices table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS devices (
                device_id TEXT PRIMARY KEY,
                vehicle_reg TEXT DEFAULT 'TN-01-AB-2026',
                device_model TEXT DEFAULT 'AI-Thinker ESP32-CAM',
                status TEXT DEFAULT 'ONLINE',
                last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        """)

        # Frames table (stores received images metadata)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS frames (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                frame_id INTEGER,
                device_id TEXT,
                filename TEXT NOT NULL,
                size_bytes INTEGER,
                latitude REAL,
                longitude REAL,
                speed_kmh REAL,
                is_gps_valid INTEGER,
                signature_verified INTEGER DEFAULT 1,
                received_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (device_id) REFERENCES devices (device_id)
            );
        """)

        # Telemetry table (GPS tracks for route playback)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT,
                latitude REAL NOT NULL,
                longitude REAL NOT NULL,
                speed_kmh REAL DEFAULT 0.0,
                heading_deg REAL DEFAULT 0.0,
                is_valid INTEGER DEFAULT 1,
                recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (device_id) REFERENCES devices (device_id)
            );
        """)

        # Violations table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS violations (
                violation_id TEXT PRIMARY KEY,
                device_id TEXT,
                violation_type TEXT NOT NULL,
                severity TEXT DEFAULT 'MEDIUM',
                confidence REAL DEFAULT 0.95,
                speed_kmh REAL,
                speed_limit_kmh REAL,
                latitude REAL,
                longitude REAL,
                evidence_image TEXT,
                evidence_hash_sha256 TEXT,
                status TEXT DEFAULT 'PENDING_REVIEW',
                remarks TEXT,
                timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (device_id) REFERENCES devices (device_id)
            );
        """)

        # Insert default demo device if not exists
        cursor.execute("""
            INSERT OR IGNORE INTO devices (device_id, vehicle_reg, device_model)
            VALUES ('TRAZEN-NODE-01', 'TN-01-AB-2026', 'AI-Thinker ESP32-CAM + NEO-6M');
        """)

        conn.commit()

def log_frame(device_id, frame_id, filename, size_bytes, latitude, longitude, speed_kmh, is_gps_valid, signature_verified=1):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("""
            INSERT INTO frames (device_id, frame_id, filename, size_bytes, latitude, longitude, speed_kmh, is_gps_valid, signature_verified)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (device_id, frame_id, filename, size_bytes, latitude, longitude, speed_kmh, 1 if is_gps_valid else 0, signature_verified))
        
        # Update device heartbeat
        cursor.execute("UPDATE devices SET last_seen = CURRENT_TIMESTAMP, status = 'ONLINE' WHERE device_id = ?", (device_id,))
        conn.commit()

def log_telemetry(device_id, latitude, longitude, speed_kmh, heading_deg, is_valid=True):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("""
            INSERT INTO telemetry (device_id, latitude, longitude, speed_kmh, heading_deg, is_valid)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (device_id, latitude, longitude, speed_kmh, heading_deg, 1 if is_valid else 0))
        conn.commit()

def log_violation(violation_id, device_id, violation_type, severity, confidence, speed_kmh, speed_limit_kmh, latitude, longitude, evidence_image, evidence_hash_sha256, remarks=None):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("""
            INSERT INTO violations (violation_id, device_id, violation_type, severity, confidence, speed_kmh, speed_limit_kmh, latitude, longitude, evidence_image, evidence_hash_sha256, remarks)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (violation_id, device_id, violation_type, severity, confidence, speed_kmh, speed_limit_kmh, latitude, longitude, evidence_image, evidence_hash_sha256, remarks))
        conn.commit()

def get_recent_violations(limit=50, violation_type=None, status=None):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        query = "SELECT * FROM violations WHERE 1=1"
        params = []
        if violation_type:
            query += " AND violation_type = ?"
            params.append(violation_type)
        if status:
            query += " AND status = ?"
            params.append(status)
        query += " ORDER BY timestamp DESC LIMIT ?"
        params.append(limit)
        cursor.execute(query, params)
        return [dict(row) for row in cursor.fetchall()]

def get_violation_by_id(violation_id):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM violations WHERE violation_id = ?", (violation_id,))
        row = cursor.fetchone()
        return dict(row) if row else None

def update_violation_status(violation_id, new_status, remarks=None):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("""
            UPDATE violations
            SET status = ?, remarks = COALESCE(?, remarks)
            WHERE violation_id = ?
        """, (new_status, remarks, violation_id))
        conn.commit()
        return cursor.rowcount > 0

def get_latest_telemetry(device_id="TRAZEN-NODE-01"):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM telemetry WHERE device_id = ? ORDER BY id DESC LIMIT 1", (device_id,))
        row = cursor.fetchone()
        return dict(row) if row else None

def get_telemetry_history(device_id="TRAZEN-NODE-01", limit=100):
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT latitude, longitude, speed_kmh, recorded_at FROM telemetry WHERE device_id = ? ORDER BY id DESC LIMIT ?", (device_id, limit))
        return [dict(row) for row in cursor.fetchall()]

def get_stats():
    with get_db_connection() as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT COUNT(*) as count FROM frames")
        total_frames = cursor.fetchone()["count"]

        cursor.execute("SELECT COUNT(*) as count FROM violations")
        total_violations = cursor.fetchone()["count"]

        cursor.execute("SELECT COUNT(*) as count FROM violations WHERE status = 'PENDING_REVIEW'")
        pending_review = cursor.fetchone()["count"]

        cursor.execute("SELECT COUNT(*) as count FROM devices WHERE status = 'ONLINE'")
        active_devices = cursor.fetchone()["count"]

        return {
            "total_frames": total_frames,
            "total_violations": total_violations,
            "pending_review": pending_review,
            "active_devices": active_devices
        }
