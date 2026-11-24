#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

#include "gps_parser.h"

// ---------------- WI-FI & SERVER SETTINGS ----------------
const char* WIFI_SSID     = "vivo Y21 5G";
const char* WIFI_PASSWORD = "jebinson(raj)";

// Active Laptop IPv4 Server Endpoint
const char* SERVER_URL    = "http://10.69.238.166:5000/api/v1/events";

// Device Authentication
const char* DEVICE_ID     = "ESP32_CAM_01";
const char* DEVICE_SECRET = "trazen-secret-key-licet-2026";

// GPS Module (NEO-6M on GPIO 13/14)
HardwareSerial GPS_Serial(2);
SimpleGPS gps;

// ---------------- CAMERA INITIALIZATION ----------------
bool startCamera() {
    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;

    config.pin_xclk = 0;
    config.pin_pclk = 22;
    config.pin_vsync = 25;
    config.pin_href = 23;

    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;

    config.pin_pwdn = 32;
    config.pin_reset = -1;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t result = esp_camera_init(&config);
    if (result != ESP_OK) {
        Serial.printf("Camera initialization failed: 0x%x\n", result);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 1);
        s->set_contrast(s, 1);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
    }

    Serial.println("Camera initialized successfully!");
    return true;
}

// ---------------- HMAC-SHA256 SIGNATURE ----------------
String createSignature(const String& metadata, const uint8_t* imageData, size_t imageLength) {
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mdInfo == nullptr) return "";

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    unsigned char hash[32];

    int result = mbedtls_md_setup(&ctx, mdInfo, 1);
    if (result == 0) {
        result = mbedtls_md_hmac_starts(&ctx, (const unsigned char*)DEVICE_SECRET, strlen(DEVICE_SECRET));
    }
    if (result == 0) {
        result = mbedtls_md_hmac_update(&ctx, (const unsigned char*)metadata.c_str(), metadata.length());
    }
    const unsigned char newline = '\n';
    if (result == 0) {
        result = mbedtls_md_hmac_update(&ctx, &newline, 1);
    }
    if (result == 0) {
        result = mbedtls_md_hmac_update(&ctx, imageData, imageLength);
    }
    if (result == 0) {
        result = mbedtls_md_hmac_finish(&ctx, hash);
    }
    mbedtls_md_free(&ctx);

    if (result != 0) return "";

    const char hex[] = "0123456789abcdef";
    String signature;
    signature.reserve(64);
    for (int i = 0; i < 32; i++) {
        signature += hex[(hash[i] >> 4) & 0x0F];
        signature += hex[hash[i] & 0x0F];
    }
    return signature;
}

// ---------------- UPLOAD FRAME ----------------
void uploadFrame(camera_fb_t* frame) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected!");
        return;
    }
    if (frame == nullptr) {
        Serial.println("No camera image!");
        return;
    }

    String eventId = "event_" + String(millis());

    // Prepare JSON Metadata
    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["event_id"] = eventId;
    doc["image_format"] = "jpeg";
    doc["width"] = frame->width;
    doc["height"] = frame->height;
    doc["latitude"] = gps.data.latitude;
    doc["longitude"] = gps.data.longitude;
    doc["speed"] = gps.data.speedKmh;
    doc["gps_valid"] = gps.data.isValid;

    String metadata;
    serializeJson(doc, metadata);

    // Compute cryptographic signature
    String signature = createSignature(metadata, frame->buf, frame->len);

    String endpoint = String(SERVER_URL) + "/" + eventId + "/frames";

    WiFiClient client;
    HTTPClient http;

    Serial.println("Uploading road frame to TRAZEN server...");

    if (!http.begin(client, endpoint)) {
        Serial.println("Could not connect to server!");
        return;
    }

    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Device-ID", DEVICE_ID);
    http.addHeader("X-Frame-Metadata", metadata);
    http.addHeader("X-Signature", signature);
    http.addHeader("X-GPS-Latitude", String(gps.data.latitude, 6));
    http.addHeader("X-GPS-Longitude", String(gps.data.longitude, 6));
    http.addHeader("X-GPS-Speed", String(gps.data.speedKmh, 2));

    int responseCode = http.POST(frame->buf, frame->len);
    if (responseCode > 0) {
        Serial.printf("[TRAZEN SERVER] Status: %d\n", responseCode);
        Serial.println(http.getString());
    } else {
        Serial.printf("[TRAZEN SERVER] Upload failed: %s\n", http.errorToString(responseCode).c_str());
    }
    http.end();
}

// ---------------- SETUP ----------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("  TRAZEN: SMART TRAFFIC VIOLATION DETECTION");
    Serial.println("  Target Server: " + String(SERVER_URL));
    Serial.println("========================================================");

    // Start GPS Serial
    GPS_Serial.begin(9600, SERIAL_8N1, 13, 14);

    // Start camera
    if (!startCamera()) {
        Serial.println("Restart board and check camera wiring.");
        return;
    }

    // Connect Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWi-Fi connected!");
        Serial.printf("ESP32 IP address: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWi-Fi connection failed!");
    }
}

// ---------------- MAIN LOOP ----------------
void loop() {
    // Feed incoming GPS serial stream
    while (GPS_Serial.available() > 0) {
        char c = (char)GPS_Serial.read();
        gps.encode(c);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Reconnecting to Wi-Fi...");
        WiFi.reconnect();
        delay(3000);
        return;
    }

    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) {
        Serial.println("Camera capture failed!");
        delay(2000);
        return;
    }

    Serial.printf("Image captured: %u bytes\n", frame->len);
    uploadFrame(frame);
    esp_camera_fb_return(frame);

    delay(2000);
}
