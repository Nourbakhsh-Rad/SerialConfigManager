/*
 * BasicUsage - SerialConfigManager
 * Compatible with ESP32 & ESP8266
 *
 * Demonstrates:
 * - Initializing the manager and its storage backend
 * - Unified port-based API (setSerialBaudRate(port, baud), etc.)
 * - Persistent configuration storage (save / load / reset)
 * - Reading back settings with the getter functions
 */

#include <SerialConfigManager.h>

SerialConfigManager scm;

void printCurrentSettings() {
    Serial.println("=== Current Serial Port Settings ===");

    for (uint8_t port = 0; port <= 1; port++) {
        Serial.printf("Port %d:\n", port);
        Serial.printf("  BaudRate: %lu\n", (unsigned long)scm.getBaudRate(port));
        Serial.printf("  DataBits: %d\n", scm.getDataBits(port));
        Serial.printf("  Parity: %d\n", scm.getParity(port));
        Serial.printf("  StopBits: %d\n", scm.getStopBits(port));
        Serial.printf("  Enabled: %s\n", scm.isEnabled(port) ? "Yes" : "No");
        Serial.printf("  Serial Active: %s\n", scm.isSerialActive(port) ? "Yes" : "No");
        Serial.println();
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.print("  SerialConfigManager v");
    Serial.println(scm.getVersion());
    Serial.println("========================================");

    // true = format LittleFS/SPIFFS/FFat if mount fails,
    // true = actually mount storage and load any saved config
    if (!scm.begin(true, true)) {
        Serial.println("Failed to initialize!");
        Serial.println("Error: " + scm.getLastError());
        return;
    }
    Serial.println("Manager initialized. isInitialized() = " +
                    String(scm.isInitialized() ? "true" : "false"));
    Serial.println();

    // --- Configure Serial0: 115200 bps, 8N1 ---
    // (Serial0 is also the console used by Serial.print() below, so keep it
    //  matching the Serial Monitor speed or the output turns to garbage.)
    scm.setSerialBaudRate(SER0, 115200);
    scm.setSerialDataBits(SER0, 8);
    scm.setSerialParity(SER0, 0);      // None
    scm.setSerialStopBits(SER0, 1);
    scm.enableSerial(SER0, true);
    scm.applySerialConfig(SER0);

    // --- Configure Serial1: 9600 bps, 8N1 ---
    scm.setSerialBaudRate(SER1, 9600);
    scm.setSerialDataBits(SER1, 8);
    scm.setSerialParity(SER1, 0);      // None
    scm.setSerialStopBits(SER1, 1);
    scm.enableSerial(SER1, true);

#if defined(ESP32)
    scm.setSerialPins(SER1, 16, 17);   // RX=GPIO16, TX=GPIO17
#else
    Serial.println("ESP8266: Serial1 is TX-only, pin setting skipped.");
#endif
    scm.applySerialConfig(SER1);
    Serial.println();

    // --- Save & verify ---
    if (scm.saveConfig()) {
        Serial.println("Config saved.");
    } else {
        Serial.println("Save failed: " + scm.getLastError());
    }
    Serial.println("configFileExists() = " +
                    String(scm.configFileExists() ? "true" : "false"));
    Serial.println();

    scm.printConfig();
    printCurrentSettings();

    Serial.println("Setup complete!");
}

void loop() {
    delay(1000);
}
