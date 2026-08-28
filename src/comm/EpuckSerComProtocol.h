#pragma once

#include <QByteArray>
#include <QImage>
#include "core/SensorData.h"

// SerCom binary/ASCII hybrid protocol for e-puck robots
// Based on BTcom.c from official EPFL e-puck firmware
class EpuckSerComProtocol {
public:
    // ---- Binary commands (negative byte values in signed char) ----
    static constexpr uint8_t CMD_SENSOR     = 0xB2; // Read proximity (8 sensors)
    static constexpr uint8_t CMD_ACCEL      = 0xBF; // Read accelerometer (spherical)
    static constexpr uint8_t CMD_MOTOR      = 0xBC; // Set motor speeds
    static constexpr uint8_t CMD_LED        = 0xB4; // Set LED
    static constexpr uint8_t CMD_IMAGE      = 0xB7; // Get camera image
    static constexpr uint8_t CMD_RAW_ACCEL  = 0xB1; // Read raw accelerometer (cartesian)
    static constexpr uint8_t CMD_MIC        = 0xF5; // Read microphone volumes (binary)
    static constexpr uint8_t CMD_GET_MOTOR  = 0xC5; // Get motor speed
    static constexpr uint8_t CMD_ENCODERS   = 0xD1; // Read encoder steps
    static constexpr uint8_t CMD_LIGHT      = 0xCE; // Read ambient light
    static constexpr uint8_t CMD_EXIT_BINARY = 0x00; // Exit binary mode

    // ---- LED numbering (matches BTcom.c / e_led.h) ----
    // 0-7: 8 ring LEDs (clockwise from front-right)
    // 8:   body LED (green, bottom)
    // 9:   front LED (red)

    // ---- LED values ----
    static constexpr int LED_OFF    = 0;
    static constexpr int LED_ON     = 1;
    static constexpr int LED_TOGGLE = 2;

    // ---- Command builders ----
    static QByteArray buildSensorPollCommand();  // [CMD_SENSOR, CMD_ACCEL, CMD_EXIT_BINARY]
    static QByteArray buildMotorCommand(int16_t left, int16_t right);
    static QByteArray buildLEDCommand(int ledNum, int value);
    static QByteArray buildImageCommand();
    static QByteArray buildRawAccelCommand();
    static QByteArray buildMicCommand();         // binary mic
    static QByteArray buildExitBinaryCommand();

    // ---- ASCII commands (must be sent after exiting binary mode) ----
    static QByteArray buildSelectorQuery();      // "C\r"
    static QByteArray buildMicAsciiQuery();      // "U\r"
    static QByteArray buildBodyLEDAscii(int value); // "B,N\r"
    static QByteArray buildFrontLEDAscii(int value);// "F,N\r"
    static QByteArray buildStopCommand();        // "S\r"
    static QByteArray buildVersionQuery();       // "V\r"
    static QByteArray buildResetCommand();       // "R\r"
    static QByteArray buildCameraParams(int mode, int w, int h, int zoom); // "J,mode,w,h,zoom\r"

    // ---- Response parsing ----
    // Parse binary sensor response into SensorData
    // Returns true if valid data was parsed
    static bool parseSensorResponse(const QByteArray &data, SensorData &out);

    // Parse camera image response
    // Returns valid QImage if successful, null QImage otherwise
    static QImage parseImageResponse(const QByteArray &data);

    // Parse ASCII response lines (e.g., "c,5\r\n", "u,100,200,300\r\n")
    static bool parseAsciiResponse(const QByteArray &line, SensorData &out);

    // ---- Device verification ----
    static QByteArray buildPingCommand();        // "\r" then "C\r"
    static bool isSelectorResponse(const QByteArray &data);

private:
    static QImage parseGreyscaleImage(const uint8_t *pixels, int w, int h);
    static QImage parseRGB565Image(const uint8_t *pixels, int w, int h);
};
