/*
 * ============================================================================
 * TRAZEN: SMART TRAFFIC VIOLATION DETECTION SYSTEM
 * Onboard Vehicular Edge Node Firmware (ESP32-CAM + NEO-6M GPS)
 * 
 * Hardware:
 *   - AI-Thinker ESP32-CAM (OV2640 2MP)
 *   - u-blox NEO-6M GPS Module
 *   - FTDI 232 USB-to-UART Programmer (for flashing)
 *   - Regulated 5V/2A DC Power Supply
 * 
 * Author: Jebinson Raj. R
 * Department: ECE, Loyola ICAM College of Engineering and Technology (LICET)
 * ============================================================================
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "mbedtls/md.h"

#include "camera_pins.h"
#include "config.h"
#include "gps_parser.h"

// Hardware Serial for NEO-6M GPS
HardwareSerial GPS_Serial(2); // UART2: RX=13, TX=14
SimpleGPS gps;

// Optional Local Web Server
#if ENABLE_LOCAL_WEB_SERVER
WebServer localServer(LOCAL_SERVER_PORT);
#endif

// State Tracking
unsigned long lastCaptureTime = 0;
unsigned long frameCounter = 0;
bool isCameraInitialized = false;

// ============================================================================
// CRYPTOGRAPHY: HMAC-SHA256 SIGNATURE GENERATOR
// Signs the raw JPEG frame to prevent spoofing and verify data integrity.
// ============================================================================
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

// ============================================================================
// CAMERA INITIALIZATION
// ============================================================================
bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Use PSRAM if available for higher frame buffer capacity
    if (psramFound()) {
        config.frame_size   = FRAMESIZE_VGA;    // 640x480 (optimal for AI speed)
        config.jpeg_quality = 12;               // 10-63 (lower = higher quality)
        config.fb_count     = 2;
    } else {
        config.frame_size   = FRAMESIZE_QVGA;   // 320x240
        config.jpeg_quality = 15;
        config.fb_count     = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] Initialization failed with error 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 1);    // -2 to 2
        s->set_contrast(s, 1);      // -2 to 2
        s->set_saturation(s, 0);    // -2 to 2
        s->set_whitebal(s, 1);      // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);      // 0 = disable , 1 = enable
        s->set_wb_mode(s, 0);       // 0 to 4 - 0: Auto
    }

    Serial.println("[CAMERA] OV2640 Camera Initialized Successfully.");
    return true;
}

// ============================================================================
// WI-FI CONNECTION HANDLER
// ============================================================================
void connectWiFi() {
    Serial.printf("\n[WIFI] Connecting to SSID: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected Successfully!");
        Serial.printf("[WIFI] Node IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("\n[WIFI] Connection Failed. Operating in offline/local mode.");
    }
}

// ============================================================================
// TRANSMIT IMAGE & TELEMETRY TO FLASK SERVER
// ============================================================================
bool sendFrameToServer(camera_fb_t *fb) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    WiFiClient client;
    if (!client.connect(SERVER_HOST, SERVER_PORT)) {
        Serial.printf("[UPLOAD] Could not connect to Server at %s:%d\n", SERVER_HOST, SERVER_PORT);
        return false;
    }

    frameCounter++;
    unsigned long currentTimestamp = millis();

    // Generate cryptographic HMAC-SHA256 signature of the frame
    String signature = generateHMAC_SHA256(fb->buf, fb->len, HMAC_SECRET_KEY);

    // Prepare HTTP Headers
    client.printf("POST %s HTTP/1.1\r\n", API_ENDPOINT_FRAME);
    client.printf("Host: %s:%d\r\n", SERVER_HOST, SERVER_PORT);
    client.printf("User-Agent: TRAZEN-ESP32-CAM/1.0\r\n");
    client.printf("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n", fb->len);
    client.printf("X-Device-Id: %s\r\n", DEVICE_ID);
    client.printf("X-Frame-Id: %lu\r\n", frameCounter);
    client.printf("X-Timestamp-Ms: %lu\r\n", currentTimestamp);
    client.printf("X-Signature-SHA256: %s\r\n", signature.c_str());
    client.printf("X-GPS-Valid: %s\r\n", gps.data.isValid ? "true" : "false");
    client.printf("X-GPS-Latitude: %.6f\r\n", gps.data.latitude);
    client.printf("X-GPS-Longitude: %.6f\r\n", gps.data.longitude);
    client.printf("X-GPS-Speed: %.2f\r\n", gps.data.speedKmh);
    client.printf("X-GPS-Heading: %.1f\r\n", gps.data.headingDeg);
    client.printf("Connection: close\r\n\r\n");

    // Transmit JPEG bytes
    client.write(fb->buf, fb->len);
    client.flush();

    // Read quick server response status
    int timeout = 0;
    while (!client.available() && timeout < 100) {
        delay(10);
        timeout++;
    }

    String responseLine = client.readStringUntil('\n');
    client.stop();

    if (responseLine.indexOf("200 OK") >= 0 || responseLine.indexOf("201") >= 0) {
        Serial.printf("[UPLOAD] Frame #%lu sent successfully (Size: %u bytes | GPS: %.4f, %.4f | Speed: %.1f km/h)\n",
                      frameCounter, fb->len, gps.data.latitude, gps.data.longitude, gps.data.speedKmh);
        return true;
    } else {
        Serial.printf("[UPLOAD] Server response: %s\n", responseLine.c_str());
        return false;
    }
}

// ============================================================================
// LOCAL WEB SERVER ENDPOINTS (FOR DIRECT BROWSER ACCESS)
// ============================================================================
#if ENABLE_LOCAL_WEB_SERVER
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>TRAZEN Node Monitor</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;background:#121826;color:#e2e8f0;padding:20px;text-align:center;}";
    html += ".card{background:#1e293b;border-radius:12px;padding:20px;max-width:500px;margin:auto;box-shadow:0 4px 6px rgba(0,0,0,0.3);}";
    html += "img{max-width:100%;border-radius:8px;border:2px solid #3b82f6;}";
    html += ".badge{display:inline-block;padding:4px 10px;border-radius:6px;font-weight:bold;margin:4px;}";
    html += ".badge-green{background:#10b981;color:#fff;}.badge-blue{background:#3b82f6;color:#fff;}";
    html += "</style></head><body><div class='card'>";
    html += "<h2>TRAZEN EDGE NODE</h2>";
    html += "<p><span class='badge badge-blue'>ID: " + String(DEVICE_ID) + "</span>";
    html += "<span class='badge badge-green'>" + (WiFi.status() == WL_CONNECTED ? "Wi-Fi ONLINE" : "OFFLINE") + "</span></p>";
    html += "<p><strong>GPS Latitude:</strong> " + String(gps.data.latitude, 6) + "<br>";
    html += "<strong>GPS Longitude:</strong> " + String(gps.data.longitude, 6) + "<br>";
    html += "<strong>Speed:</strong> " + String(gps.data.speedKmh, 1) + " km/h | <strong>Fix:</strong> " + (gps.data.isValid ? "LOCKED" : "SEARCHING") + "</p>";
    html += "<p><img id='cam' src='/capture' alt='Camera Stream'></p>";
    html += "<p><button onclick=\"document.getElementById('cam').src='/capture?r=' + Math.random()\" style='padding:8px 16px;cursor:pointer;border-radius:6px;border:none;background:#3b82f6;color:white;'>Refresh Frame</button></p>";
    html += "<p><small>Loyola ICAM College of Engineering & Technology | 2026</small></p>";
    html += "</div></body></html>";
    localServer.send(200, "text/html", html);
}

void handleCapture() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        localServer.send(500, "text/plain", "Camera Capture Failed");
        return;
    }
    localServer.setContentLength(fb->len);
    localServer.send(200, "image/jpeg", "");
    WiFiClient client = localServer.client();
    client.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void handleStatus() {
    String json = "{";
    json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"gps_valid\":" + String(gps.data.isValid ? "true" : "false") + ",";
    json += "\"latitude\":" + String(gps.data.latitude, 6) + ",";
    json += "\"longitude\":" + String(gps.data.longitude, 6) + ",";
    json += "\"speed_kmh\":" + String(gps.data.speedKmh, 2) + ",";
    json += "\"frames_captured\":" + String(frameCounter) + "}";
    localServer.send(200, "application/json", json);
}
#endif

// ============================================================================
// ARDUINO SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================================");
    Serial.println("  TRAZEN: SMART TRAFFIC VIOLATION DETECTION NODE");
    Serial.println("  Department of ECE, LICET (2026-2027)");
    Serial.println("========================================================");

    // Initialize Status LED (Red LED on ESP32-CAM)
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH); // OFF initially (inverted)

    // Initialize GPS UART
    GPS_Serial.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.printf("[GPS] NEO-6M Serial initialized on RX=%d, TX=%d @ %d baud\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);

    // Initialize Camera
    isCameraInitialized = initCamera();
    if (!isCameraInitialized) {
        Serial.println("[ERROR] Halting execution due to camera failure.");
        while (true) {
            digitalWrite(STATUS_LED_PIN, LOW); // Blink rapidly
            delay(100);
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(100);
        }
    }

    // Connect to Wi-Fi
    connectWiFi();

    // Start Local Web Server
#if ENABLE_LOCAL_WEB_SERVER
    localServer.on("/", HTTP_GET, handleRoot);
    localServer.on("/capture", HTTP_GET, handleCapture);
    localServer.on("/status", HTTP_GET, handleStatus);
    localServer.begin();
    Serial.printf("[LOCAL WEB] Direct web monitor running at http://%s/\n", WiFi.localIP().toString().c_str());
#endif

    // Ready signal: Blink LED twice
    for (int i = 0; i < 2; i++) {
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(150);
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(150);
    }
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================
void loop() {
    // 1. Ingest GPS data from NEO-6M Serial Stream
    while (GPS_Serial.available() > 0) {
        char c = (char)GPS_Serial.read();
        gps.encode(c);
    }

    // 2. Handle incoming requests on the local web server
#if ENABLE_LOCAL_WEB_SERVER
    localServer.handleClient();
#endif

    // 3. Periodic Frame Capture and Server Upload
    unsigned long now = millis();
    if (now - lastCaptureTime >= CAPTURE_INTERVAL_MS) {
        lastCaptureTime = now;

        // Turn status LED ON during capture
        digitalWrite(STATUS_LED_PIN, LOW);

        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            sendFrameToServer(fb);
            esp_camera_fb_return(fb);
        } else {
            Serial.println("[CAMERA] Capture failed.");
        }

        // Turn status LED back OFF
        digitalWrite(STATUS_LED_PIN, HIGH);
    }

    // 4. Wi-Fi Auto-Reconnect Safeguard
    if (WiFi.status() != WL_CONNECTED && (now % 10000 < 50)) {
        Serial.println("[WIFI] Reconnecting...");
        WiFi.reconnect();
    }
}
