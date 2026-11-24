#ifndef TRAZEN_CONFIG_H
#define TRAZEN_CONFIG_H

// ============================================================================
// TRAZEN: ESP32-CAM & GPS CONFIGURATION FILE
// Update these values to match your Wi-Fi network and laptop/server IP.
// ============================================================================

// --- Wi-Fi Settings ---
const char* const WIFI_SSID     = "vivo Y21 5G";
const char* const WIFI_PASSWORD = "jebinson(raj)";

// --- Server Settings ---
const char* const SERVER_HOST   = "10.69.238.166";
const int         SERVER_PORT   = 5000;

// API Endpoints
const char* const API_ENDPOINT_FRAME      = "/api/v1/frames";
const char* const API_ENDPOINT_TELEMETRY  = "/api/v1/telemetry";

// --- Device Authentication & Cryptography ---
const char* const DEVICE_ID       = "TRAZEN-NODE-01";
const char* const HMAC_SECRET_KEY = "trazen-secret-key-licet-2026";

// --- GPS Hardware Configuration (NEO-6M) ---
// Connect NEO-6M TX pin -> ESP32-CAM GPIO 13 (ESP32 RX)
// Connect NEO-6M RX pin -> ESP32-CAM GPIO 14 (ESP32 TX)
#define GPS_RX_PIN 13
#define GPS_TX_PIN 14
#define GPS_BAUD_RATE 9600

// --- Capture & Transmission Timing ---
// Upload frequency in milliseconds (1000 ms = 1 frame/sec)
#define CAPTURE_INTERVAL_MS 1500

// Set to true to start a local web server (http://<esp32-ip>/) for direct viewing
#define ENABLE_LOCAL_WEB_SERVER true
#define LOCAL_SERVER_PORT 80

#endif // TRAZEN_CONFIG_H
