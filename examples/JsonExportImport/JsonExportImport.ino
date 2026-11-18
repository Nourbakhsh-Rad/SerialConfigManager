/*
 * JsonExportImport - SerialConfigManager
 * Compatible with ESP32 & ESP8266
 *
 * Demonstrates:
 * - Exporting the current configuration as a JSON string (configToJson)
 * - Importing configuration from a JSON string (parseJsonConfig), which
 *   validates every field and rejects the update if anything is invalid
 * - Managing the saved config file (configFileExists / deleteConfigFile)
 */

#include <SerialConfigManager.h>

SerialConfigManager scm;

void setup() {
    Serial.begin(115200);
    delay(1000);

    scm.begin(true, true);

    // Change Serial1 (not Serial0): Serial0 is the console used by Serial.print().
    scm.setSerialBaudRate(SER1, 57600);
    scm.setSerialParity(SER1, 2); // Even

    String exported = scm.configToJson();
    Serial.println("--- Exported config ---");
    Serial.println(exported);

    // Round-trip it back in (could equally come from a web UI, a config
    // server, an SD card, etc.)
    if (scm.parseJsonConfig(exported)) {
        Serial.println("Re-imported successfully.");
    } else {
        Serial.println("Import failed: " + scm.getLastError());
    }

    // A payload with an out-of-range field is rejected outright and the
    // previous in-memory configuration is left untouched:
    String badJson =
        "{\"serial0\":{\"baudRate\":999999,\"dataBits\":8,\"parity\":0,"
        "\"stopBits\":1,\"enabled\":true},"
        "\"serial1\":{\"baudRate\":9600,\"dataBits\":8,\"parity\":0,"
        "\"stopBits\":1,\"enabled\":true}}";
    if (!scm.parseJsonConfig(badJson)) {
        Serial.println("Correctly rejected invalid config: " + scm.getLastError());
    }

    scm.applySerialConfig(SER0);
    scm.applySerialConfig(SER1);
    scm.saveConfig();

    Serial.println("configFileExists() = " +
                    String(scm.configFileExists() ? "true" : "false"));

    // Uncomment to erase the saved config:
    // scm.deleteConfigFile();
}

void loop() {
    delay(1000);
}
