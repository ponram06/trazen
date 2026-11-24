import os
from pathlib import Path

# Base directories
BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"
UPLOAD_DIR = STATIC_DIR / "uploads"
FRAMES_DIR = UPLOAD_DIR / "frames"
VIOLATIONS_DIR = UPLOAD_DIR / "violations"
DB_PATH = BASE_DIR / "trazen.db"

# Ensure directories exist
FRAMES_DIR.mkdir(parents=True, exist_ok=True)
VIOLATIONS_DIR.mkdir(parents=True, exist_ok=True)

# Server settings
HOST = "0.0.0.0"
PORT = 5000
DEBUG = True

# Security & Authentications
SECRET_KEY = os.environ.get("TRAZEN_SECRET_KEY", "trazen-secret-key-licet-2026")
DEFAULT_DEVICE_ID = "TRAZEN-NODE-01"

# Violation thresholds
DEFAULT_SPEED_LIMIT_KMH = 40.0
SPEED_TOLERANCE_KMH = 5.0
