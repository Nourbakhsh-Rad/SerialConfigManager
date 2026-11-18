/*
 * AutoDirectionSend - SerialConfigManager
 * Compatible with ESP32 & ESP8266
 *
 * The point of this example: the sending code is IDENTICAL for a plain
 * UART port and an RS-485 port. Call scm.writeSerial(port, data) and the
 * library figures out on its own whether DE/RE direction switching is
 * needed - you never have to check isRS485Enabled() yourself or branch
 * your sending code based on it.
 */

#include <SerialConfigManager.h>

SerialConfigManager scm;

void sendStatusMessage(uint8_t port, const String& message);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // true = format on mount failure, true = let begin() mount storage
    // itself. initStorage=false is NOT an "in-memory only" mode - it means
    // "I already called SCM_FS.begin()/EEPROM.begin() myself" - so unless
    // this sketch does that elsewhere, storage must be initialized here.
    scm.begin(true, true);

    // --- Serial0: a perfectly plain UART port, no RS-485 involved ---
    // (kept at 115200 = the Serial Monitor speed, since Serial.println() below uses it)
    scm.setSerialBaudRate(SER0, 115200);
    scm.enableSerial(SER0, true);
    scm.applySerialConfig(SER0);

    // --- Serial1: same kind of setup, but wired for RS-485 half-duplex ---
    scm.setSerialBaudRate(SER1, 115200);
    scm.enableSerial(SER1, true);
#if defined(ESP32)
    scm.setSerialPins(SER1, 16, 17);     // RX=GPIO16, TX=GPIO17 - pick pins valid for YOUR board
#endif
    scm.setRS485Pins(SER1, 18, 19);      // DE=GPIO18, RE=GPIO19
    scm.enableRS485(SER1, true);         // <- the only RS-485-specific call anywhere
    scm.applySerialConfig(SER1);

    // From here on, sending looks exactly the same regardless of port:
    sendStatusMessage(SER0, "Hello from plain UART!");
    sendStatusMessage(SER1, "Hello from RS-485!");

    // writeSerial() also accepts a plain C-string directly - no need to
    // compute or pass a length yourself:
    scm.writeSerial(SER0, "No length needed here either.");
}

// This function has no idea (and doesn't need to know) whether "port" is
// RS-485 or not. writeSerial() automatically:
//  - does nothing extra for a plain port (just a timed write), or
//  - switches DE/RE to transmit, writes, drains, then switches back to
//    receive, if RS-485 is enabled for that port.
void sendStatusMessage(uint8_t port, const String& message) {
    if (scm.writeSerial(port, message, 500)) {
        Serial.println("[port " + String(port) + "] sent: " + message);
    } else {
        Serial.println("[port " + String(port) + "] send failed: " + scm.getLastError());
    }
}

void loop() {
    delay(1000);
}
