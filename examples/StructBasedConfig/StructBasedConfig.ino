/*
 * StructBasedConfig - SerialConfigManager
 * Compatible with ESP32 & ESP8266
 *
 * Demonstrates configuring a port in one call by filling a SerialConfigs
 * struct and passing it to setSerialConfig(), instead of calling the
 * individual setters (setSerialBaudRate, setSerialDataBits, ...) one by
 * one. Useful when a full configuration is already available as a unit -
 * e.g. built from a settings screen, received from another device, or
 * simply as a compact way to declare a whole port's settings at once.
 *
 * Note: setSerialConfig() validates baudRate/dataBits/parity/stopBits
 * and returns false (with a reason in getLastError()) if any of them is
 * invalid. Fields not set explicitly below fall back to whatever the
 * SerialConfigs default constructor initializes them to.
 */

#include <SerialConfigManager.h>

SerialConfigManager scm;

void setup() {
    Serial.begin(115200);
    delay(1000);

    scm.begin(true, true);

    // --- Serial0: plain UART, configured entirely via the struct ---
    SerialConfigs cfg0;
    cfg0.baudRate     = 115200; // = Serial Monitor speed (Serial0 is also the console)
    cfg0.dataBits     = 8;
    cfg0.parity       = 0;      // 0=None, 1=Odd, 2=Even
    cfg0.stopBits     = 1;
    cfg0.enabled      = true;
    cfg0.rxEnabled    = true;
    cfg0.txEnabled    = true;
    cfg0.rxPin        = -1;     // -1 = platform default
    cfg0.txPin        = -1;
    cfg0.rs485Mode    = SCM_RS485_NONE;

    if (scm.setSerialConfig(SER0, cfg0)) {
        scm.applySerialConfig(SER0);
        Serial.println("Serial0 configured via struct.");
    } else {
        Serial.println("Serial0 config rejected: " + scm.getLastError());
    }

    // --- Serial1: RS-485 half-duplex, also fully described in one struct ---
    SerialConfigs cfg1;
    cfg1.baudRate     = 115200;
    cfg1.dataBits     = 8;
    cfg1.parity       = 0;
    cfg1.stopBits     = 1;
    cfg1.enabled      = true;
#if defined(ESP32)
    cfg1.rxPin        = 16;
    cfg1.txPin        = 17;
#endif
    cfg1.dePin        = 18;     // Driver Enable
    cfg1.rePin        = 19;     // Receiver Enable
    cfg1.rs485Mode    = SCM_RS485_HALF_DUPLEX;
    cfg1.rs485Invert  = false;  // active-high DE/RE

    if (scm.setSerialConfig(SER1, cfg1)) {
        scm.applySerialConfig(SER1);
        Serial.println("Serial1 (RS-485) configured via struct.");
    } else {
        Serial.println("Serial1 config rejected: " + scm.getLastError());
    }

    // setSerialConfig() only updates the in-memory config; persist it
    // explicitly if you want it to survive a reboot.
    if (scm.saveConfig()) {
        Serial.println("Configuration saved.");
    }

    scm.printConfig();
}

void loop() {
    delay(1000);
}
