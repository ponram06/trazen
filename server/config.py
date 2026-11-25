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
DEBUG = os.environ.get("TRAZEN_DEBUG", "false").lower() == "true"

# Security & Authentications
SECRET_KEY = os.environ.get("TRAZEN_SECRET_KEY")
DEFAULT_DEVICE_ID = "TRAZEN-NODE-01"

if not SECRET_KEY:
    raise RuntimeError(
        "TRAZEN_SECRET_KEY is required. Set it before starting the server; "
        "use the identical value in firmware/esp32_cam/config.local.h."
    )

# Violation thresholds
DEFAULT_SPEED_LIMIT_KMH = 40.0
SPEED_TOLERANCE_KMH = 5.0
