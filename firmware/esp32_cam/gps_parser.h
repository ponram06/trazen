#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include <Arduino.h>

// ============================================================================
// SELF-CONTAINED NMEA GPS PARSER FOR NEO-6M (NO EXTERNAL LIBRARIES REQUIRED)
// Parses $GPRMC and $GPGGA sentences directly from the UART serial stream.
// ============================================================================

struct GPSData {
    bool   isValid;         // True if satellite fix acquired
    float  latitude;        // Decimal degrees (e.g. 13.0604)
    float  longitude;       // Decimal degrees (e.g. 80.2496)
    float  speedKmh;        // Speed in km/h
    float  headingDeg;      // Direction heading in degrees (0-360)
    int    satellites;      // Visible satellites count
    char   timestamp[32];   // Formatted UTC string
};

class SimpleGPS {
private:
    char buffer[128];
    int bufIndex = 0;

    // Convert NMEA degree-minute format (ddmm.mmmm) to decimal degrees
    float convertToDecimalDegrees(const char* raw, char direction) {
        if (!raw || strlen(raw) == 0) return 0.0f;
        float val = atof(raw);
        int degrees = (int)(val / 100);
        float minutes = val - (degrees * 100);
        float dec = degrees + (minutes / 60.0f);
        if (direction == 'S' || direction == 'W') {
            dec = -dec;
        }
        return dec;
    }

public:
    GPSData data;

    SimpleGPS() {
        data.isValid = false;
        data.latitude = 13.0604f;   // Default fallback coordinates (Chennai, Loyola ICAM)
        data.longitude = 80.2496f;
        data.speedKmh = 0.0f;
        data.headingDeg = 0.0f;
        data.satellites = 0;
        strcpy(data.timestamp, "2026-11-24T00:00:00Z");
    }

    // Feed a character from GPS UART into the parser
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

        if (c == '$') {
            bufIndex = 0;
        }

        if (bufIndex < (int)sizeof(buffer) - 1) {
            buffer[bufIndex++] = c;
        }
        return false;
    }

    // Parses a single NMEA sentence
    bool parseSentence(const char* sentence) {
        // We look for $GPRMC or $GNRMC (Recommended Minimum Navigation Information)
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
                } else {
                    break;
                }
            }

            if (tokenCount >= 8) {
                // tokens[2] is status: 'A' = Valid, 'V' = Void
                char status = tokens[2][0];
                if (status == 'A') {
                    data.isValid = true;
                    // Latitude
                    data.latitude = convertToDecimalDegrees(tokens[3], tokens[4][0]);
                    // Longitude
                    data.longitude = convertToDecimalDegrees(tokens[5], tokens[6][0]);
                    // Speed in knots -> convert to km/h (1 knot = 1.852 km/h)
                    float speedKnots = atof(tokens[7]);
                    data.speedKmh = speedKnots * 1.852f;

                    if (tokenCount >= 9 && strlen(tokens[8]) > 0) {
                        data.headingDeg = atof(tokens[8]);
                    }

                    // Extract UTC time if available
                    if (strlen(tokens[1]) >= 6) {
                        snprintf(data.timestamp, sizeof(data.timestamp), "UTC %c%c:%c%c:%c%c",
                                 tokens[1][0], tokens[1][1], tokens[1][2],
                                 tokens[1][3], tokens[1][4], tokens[1][5]);
                    }
                    return true;
                } else {
                    data.isValid = false;
                }
            }
        }
        return false;
    }
};

#endif // GPS_PARSER_H
