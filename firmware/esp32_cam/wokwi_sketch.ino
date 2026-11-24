/*
 * ============================================================================
 * TRAZEN: SMART TRAFFIC VIOLATION DETECTION SYSTEM
 * Wokwi Edge Node Simulation Sketch (ESP32 / ESP32-CAM + NEO-6M GPS)
 *
 * Simulates:
 *   - AI-Thinker ESP32-CAM with OV2640 frame acquisition / synthetic stream
 *   - NEO-6M GPS NMEA telemetry decoding (Loyola ICAM Campus, Chennai)
 *   - HMAC-SHA256 cryptographic frame signing
 *   - Virtual Wi-Fi connectivity (Wokwi-GUEST)
 *   - Local Edge Node Web Monitor (WebServer on port 80)
 * ============================================================================
 */

#if __has_include("esp_camera.h")
#include "esp_camera.h"
#define HAS_ESP_CAMERA 1
#else
#define HAS_ESP_CAMERA 0
#endif

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "mbedtls/md.h"

// ---------------------------------------------------------------------------
// 1. HARDWARE PIN DEFINITIONS & CONFIGURATION
// ---------------------------------------------------------------------------
#define STATUS_LED_PIN    2      // Standard ESP32 onboard LED (also GPIO 33 for ESP32-CAM)
#define ALT_LED_PIN       33     // ESP32-CAM red LED

// Wokwi Virtual Wi-Fi Configuration
const char* const WIFI_SSID     = "Wokwi-GUEST";
const char* const WIFI_PASSWORD = "";

// Laptop / Cloud Server Settings
const char* const SERVER_HOST   = "192.168.1.100";
const int         SERVER_PORT   = 5000;
const char* const API_ENDPOINT_FRAME = "/api/v1/frames";

// Device Cryptography & Authentication
const char* const DEVICE_ID       = "TRAZEN-NODE-01";
const char* const HMAC_SECRET_KEY = "trazen-secret-key-licet-2026";

// Timing Configuration
#define CAPTURE_INTERVAL_MS 2000
#define GPS_UPDATE_INTERVAL_MS 1000

// ---------------------------------------------------------------------------
// 2. GPS TELEMETRY DECODER (NMEA $GPRMC PARSER)
// ---------------------------------------------------------------------------
struct GPSData {
    bool   isValid;
    float  latitude;
    float  longitude;
    float  speedKmh;
    float  headingDeg;
    char   timestamp[32];
};

class SimpleGPS {
private:
    char buffer[128];
    int bufIndex = 0;

    float convertToDecimalDegrees(const char* raw, char direction) {
        if (!raw || strlen(raw) == 0) return 0.0f;
        float val = atof(raw);
        int degrees = (int)(val / 100);
        float minutes = val - (degrees * 100);
        float dec = degrees + (minutes / 60.0f);
        if (direction == 'S' || direction == 'W') dec = -dec;
        return dec;
    }

public:
    GPSData data;

    SimpleGPS() {
        data.isValid = false;
        data.latitude = 13.06042f;   // LICET Chennai
        data.longitude = 80.24965f;
        data.speedKmh = 36.5f;
        data.headingDeg = 75.0f;
        strcpy(data.timestamp, "UTC 12:30:00");
    }

    bool encode(char c) {
        if (c == '\r' || c == '\n') {
            if (bufIndex > 0) {
                buffer[bufIndex] = '\0';
                bool parsed = parseSentence(buffer);
                bufIndex = 0;
                return parsed;
            }
            return false;
        }
        if (c == '$') bufIndex = 0;
        if (bufIndex < (int)sizeof(buffer) - 1) buffer[bufIndex++] = c;
        return false;
    }

    bool parseSentence(const char* sentence) {
        if (strstr(sentence, "RMC") != nullptr) {
            char temp[128];
            strncpy(temp, sentence, sizeof(temp));
            temp[sizeof(temp) - 1] = '\0';

            char* tokens[14];
            int tokenCount = 0;
            char* ptr = temp;

            while (tokenCount < 14 && ptr) {
                tokens[tokenCount++] = ptr;
                char* comma = strchr(ptr, ',');
                if (comma) {
                    *comma = '\0';
                    ptr = comma + 1;
                } else break;
            }

            if (tokenCount >= 8) {
                char status = tokens[2][0];
                if (status == 'A') {
                    data.isValid = true;
                    data.latitude = convertToDecimalDegrees(tokens[3], tokens[4][0]);
                    data.longitude = convertToDecimalDegrees(tokens[5], tokens[6][0]);
                    float speedKnots = atof(tokens[7]);
                    data.speedKmh = speedKnots * 1.852f;
                    if (tokenCount >= 9 && strlen(tokens[8]) > 0) {
                        data.headingDeg = atof(tokens[8]);
                    }
                    return true;
                }
            }
        }
        return false;
    }
};

SimpleGPS gps;
WebServer localServer(80);

unsigned long lastCaptureTime = 0;
unsigned long lastGpsSimTime = 0;
unsigned long frameCounter = 0;
bool isCameraInitialized = false;
float simAngle = 0.0f;

// Minimal valid JPEG stream for virtual frame transmission (134 bytes)
static const uint8_t mock_jpeg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x48,
    0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08,
    0x07, 0x07, 0x07, 0x09, 0x09, 0x08, 0x0a, 0x0c, 0x14, 0x0d, 0x0c, 0x0b, 0x0b, 0x0c, 0x19, 0x12,
    0x13, 0x0f, 0x14, 0x1d, 0x1a, 0x1f, 0x1e, 0x1d, 0x1a, 0x1c, 0x1c, 0x20, 0x24, 0x2e, 0x27, 0x20,
    0x22, 0x2c, 0x23, 0x1c, 0x1c, 0x28, 0x37, 0x29, 0x2c, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1f, 0x27,
    0x39, 0x3d, 0x38, 0x32, 0x3c, 0x2e, 0x33, 0x34, 0x32, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x1f, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3f,
    0x00, 0x7f, 0x00, 0xff, 0xd9
};
static const size_t mock_jpeg_len = sizeof(mock_jpeg);

// ---------------------------------------------------------------------------
// 3. CRYPTOGRAPHY: HMAC-SHA256 SIGNATURE GENERATION
// ---------------------------------------------------------------------------
String generateHMAC_SHA256(const uint8_t *payload, size_t length, const char *key) {
    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, strlen(key));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)payload, length);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    char hexBuffer[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&hexBuffer[i * 2], "%02x", (unsigned int)hmacResult[i]);
    }
    hexBuffer[64] = '\0';
    return String(hexBuffer);
}

// ---------------------------------------------------------------------------
// 4. CAMERA INITIALIZATION / SIMULATION FALLBACK
// ---------------------------------------------------------------------------
bool initCamera() {
#if HAS_ESP_CAMERA
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = 5;
    config.pin_d1       = 18;
    config.pin_d2       = 19;
    config.pin_d3       = 21;
    config.pin_d4       = 36;
    config.pin_d5       = 39;
    config.pin_d6       = 34;
    config.pin_d7       = 35;
    config.pin_xclk     = 0;
    config.pin_pclk     = 22;
    config.pin_vsync    = 25;
    config.pin_href     = 23;
    config.pin_sscb_sda = 26;
    config.pin_sscb_scl = 27;
    config.pin_pwdn     = 32;
    config.pin_reset    = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err == ESP_OK) {
        Serial.println("[CAMERA] Physical OV2640 sensor initialized.");
        return true;
    }
    Serial.printf("[CAMERA] Physical sensor not present (0x%x). Running in Wokwi Synthetic Mode.\n", err);
#else
    Serial.println("[CAMERA] Running in Synthetic Camera Mode.");
#endif
    return true; // Graceful fallback
}

// ---------------------------------------------------------------------------
// 5. WI-FI CONNECTION (WOKWI VIRTUAL AP)
// ---------------------------------------------------------------------------
void connectWiFi() {
    Serial.printf("[WIFI] Connecting to Wokwi Access Point: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(300);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected Successfully to Virtual Network!");
        Serial.printf("[WIFI] Edge Node IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    } else {
        Serial.println("\n[WIFI] Running in offline standalone simulation mode.");
    }
}

// ---------------------------------------------------------------------------
// 6. LOCAL WEB MONITOR SERVER
// ---------------------------------------------------------------------------
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>TRAZEN Edge Node</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Segoe UI,sans-serif;background:#0f172a;color:#f8fafc;padding:24px;text-align:center;}";
    html += ".card{background:#1e293b;border-radius:12px;padding:24px;max-width:520px;margin:auto;box-shadow:0 10px 15px -3px rgba(0,0,0,0.5);border:1px solid #334155;}";
    html += "h1{color:#38bdf8;font-size:22px;margin-bottom:8px;}";
    html += ".badge{display:inline-block;padding:5px 12px;border-radius:20px;font-weight:600;font-size:12px;margin:4px;}";
    html += ".badge-green{background:#065f46;color:#34d399;}.badge-blue{background:#1e40af;color:#93c5fd;}";
    html += ".telemetry{text-align:left;background:#0f172a;border-radius:8px;padding:12px;margin:16px 0;font-family:monospace;font-size:13px;line-height:1.7;}";
    html += "</style></head><body><div class='card'>";
    html += "<h1>TRAZEN EDGE NODE SIMULATION</h1>";
    html += "<p><span class='badge badge-blue'>DEVICE: " + String(DEVICE_ID) + "</span> ";
    html += "<span class='badge badge-green'>STATUS: OPERATIONAL</span></p>";
    html += "<div class='telemetry'>";
    html += "<div><strong>GPS Fix:</strong> " + String(gps.data.isValid ? "LOCKED (3D Fix)" : "SEARCHING") + "</div>";
    html += "<div><strong>Latitude:</strong> " + String(gps.data.latitude, 6) + " N (LICET Chennai)</div>";
    html += "<div><strong>Longitude:</strong> " + String(gps.data.longitude, 6) + " E</div>";
    html += "<div><strong>Speed:</strong> " + String(gps.data.speedKmh, 1) + " km/h</div>";
    html += "<div><strong>Frames Captured:</strong> " + String(frameCounter) + "</div>";
    html += "</div>";
    html += "<p style='color:#94a3b8;font-size:12px;'>Loyola ICAM College of Engineering and Technology</p>";
    html += "</div></body></html>";
    localServer.send(200, "text/html", html);
}

void handleStatus() {
    String json = "{";
    json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
    json += "\"wifi_status\":\"" + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED") + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"gps_valid\":" + String(gps.data.isValid ? "true" : "false") + ",";
    json += "\"latitude\":" + String(gps.data.latitude, 6) + ",";
    json += "\"longitude\":" + String(gps.data.longitude, 6) + ",";
    json += "\"speed_kmh\":" + String(gps.data.speedKmh, 2) + ",";
    json += "\"frames_captured\":" + String(frameCounter) + "}";
    localServer.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// 7. SETUP & MAIN EXECUTION LOOP
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================================");
    Serial.println("  TRAZEN: SMART TRAFFIC VIOLATION EDGE SIMULATION");
    Serial.println("  Dept of ECE, LICET Chennai | Online Wokwi Run");
    Serial.println("========================================================");

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Initialize Camera / Mock Fallback
    isCameraInitialized = initCamera();

    // Connect to Virtual Wi-Fi
    connectWiFi();

    // Start Local Web Server
    localServer.on("/", HTTP_GET, handleRoot);
    localServer.on("/status", HTTP_GET, handleStatus);
    localServer.begin();
    Serial.printf("[SERVER] Local Web Monitor active on http://%s/\n", WiFi.localIP().toString().c_str());

    // Flash LED twice to confirm node ready
    for (int i = 0; i < 2; i++) {
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(120);
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(120);
    }
}

void loop() {
    unsigned long now = millis();

    // 1. Simulate Incoming NMEA GPS Stream (Vehicular movement around LICET)
    if (now - lastGpsSimTime >= GPS_UPDATE_INTERVAL_MS) {
        lastGpsSimTime = now;
        simAngle += 0.08f;

        // Circular vehicular trajectory around campus
        float lat = 13.06042f + (0.0020f * sin(simAngle));
        float lon = 80.24965f + (0.0020f * cos(simAngle));
        float speed = 32.0f + (6.0f * sin(simAngle * 2.0f));

        // Format realistic NMEA $GPRMC string
        char nmeaSentence[100];
        snprintf(nmeaSentence, sizeof(nmeaSentence),
                 "$GPRMC,123519,A,%02d%07.4f,N,%03d%07.4f,E,%.1f,%.1f,241126,,,A*77\r\n",
                 (int)lat, (lat - (int)lat) * 60.0f,
                 (int)lon, (lon - (int)lon) * 60.0f,
                 speed / 1.852f, fmod(simAngle * 57.295f, 360.0f));

        // Feed sentence into GPS decoder
        for (size_t i = 0; i < strlen(nmeaSentence); i++) {
            gps.encode(nmeaSentence[i]);
        }
    }

    // 2. Handle incoming client requests on local server
    localServer.handleClient();

    // 3. Periodic Frame Capture, Cryptographic Signing & Telemetry Logging
    if (now - lastCaptureTime >= CAPTURE_INTERVAL_MS) {
        lastCaptureTime = now;
        frameCounter++;

        // Blink Status LED to signify frame capture
        digitalWrite(STATUS_LED_PIN, HIGH);

        // Compute HMAC-SHA256 signature of the frame
        String sig = generateHMAC_SHA256(mock_jpeg, mock_jpeg_len, HMAC_SECRET_KEY);

        // Check for simulated violation condition
        bool speedAlert = (gps.data.speedKmh > 36.0f);

        Serial.printf("[EDGE NODE] Frame #%03lu | Size: %uB | SHA256: %s...\n",
                      frameCounter, (unsigned int)mock_jpeg_len, sig.substring(0, 16).c_str());
        Serial.printf("            GPS: Lat %.5f, Lon %.5f | Speed: %.1f km/h | Status: %s\n",
                      gps.data.latitude, gps.data.longitude, gps.data.speedKmh,
                      speedAlert ? "🚨 SPEED ALERT!" : "NORMAL CRUISE");

        delay(50);
        digitalWrite(STATUS_LED_PIN, LOW);
    }
}
