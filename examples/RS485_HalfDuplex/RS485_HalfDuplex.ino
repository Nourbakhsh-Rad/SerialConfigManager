/*
 * RS485_HalfDuplex - SerialConfigManager
 * Compatible with ESP32 & ESP8266
 *
 * Demonstrates:
 * - RS-485 half-duplex direction control with DE/RE pins
 * - Automatic direction switching via writeRS485() with a timeout
 * - Manual direction control via setTransmitMode()/setReceiveMode()
 */

#include <SerialConfigManager.h>

SerialConfigManager scm;

void setup() {
    Serial.begin(115200);
    delay(1000);

    // true = format on mount failure, true = let begin() mount storage
    // itself (this example doesn't call SCM_FS.begin() on its own)
    scm.begin(true, true);

    Serial.println("--- RS-485 Half-Duplex Configuration ---");
    Serial.println("Serial1: 115200 bps, 8N1, DE=GPIO18, RE=GPIO19");

    scm.setSerialBaudRate(SER1, 115200);
    scm.setSerialDataBits(SER1, 8);
    scm.setSerialParity(SER1, 0);
    scm.setSerialStopBits(SER1, 1);
    scm.enableSerial(SER1, true);
#if defined(ESP32)
    // ESP32: always set explicit pins for Serial1 (its default pins may be
    // wired to flash on some boards). Pick pins valid for YOUR board.
    scm.setSerialPins(SER1, 16, 17);   // RX=GPIO16, TX=GPIO17
#endif

    // DE = GPIO18, RE = GPIO19 (tie both to the same pin if your
    // transceiver only exposes one combined direction pin)
    scm.setRS485Pins(SER1, 18, 19);
    scm.setRS485Mode(SER1, SCM_RS485_HALF_DUPLEX);
    scm.setRS485Invert(SER1, false);   // active-high DE/RE
    scm.enableRS485(SER1, true);

    scm.applySerialConfig(SER1);

    // writeRS485() automatically switches to transmit, sends, then
    // returns the bus to receive mode.
    if (scm.writeRS485(SER1, "Hello RS-485 Network!\r\n", 1000)) {
        Serial.println("Sent: Hello RS-485 Network!");
    } else {
        Serial.println("Send failed (timeout or error): " + scm.getLastError());
    }
}

void loop() {
    // After writeRS485(), the port is automatically back in receive mode.
    // Just read normally.
#if defined(ESP32)
    while (Serial1.available()) {
        char c = Serial1.read();
        Serial.write(c);  // Echo to USB Serial
    }
#endif

    // Manual direction control example (no-ops in full-duplex mode):
    // scm.setTransmitMode(SER1);
    // Serial1.write("Manual TX\r\n");
    // Serial1.flush();
    // scm.setReceiveMode(SER1);

    delay(10);
}
