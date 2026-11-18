/**
 * @file SerialConfigManager.h
 * @brief Unified configuration and persistence manager for the two
 *        hardware UARTs (Serial / Serial1) available on ESP32 & ESP8266,
 *        with optional RS-485 half/full-duplex direction control.
 *
 * @see SCM_LIB_VERSION for the current release version.
 * @see README.md for the full API reference and usage examples.
 */

#ifndef SERIAL_CONFIG_MANAGER_H
#define SERIAL_CONFIG_MANAGER_H

#include <Arduino.h>
#include "SerialCfgDefs.h"

#if (defined(ESP8266) || defined(ESP32))
    #include <FS.h>
    
    #if defined(ESP8266) && (SCM_FS_MODE == 2)
        #error [SerialConfigManager.h] ESP8266 does not support FFat!
    #endif
    
    #if   (SCM_FS_MODE == 1)
        #include <LittleFS.h>
        #define SCM_FS               LittleFS
    #elif (SCM_FS_MODE == 2)
        #include <FFat.h>
        #define SCM_FS               FFat
    #else
        #if defined(ESP32)
            #include <SPIFFS.h>
        #endif
        #define SCM_FS               SPIFFS
    #endif
#else
    #error [SerialConfigManager.h] not supported MCU !!!
#endif


/**
 * @brief Full configuration for a single UART port.
 *
 * Default-constructed with SCM_DEFAULT_* values (see SerialCfgDefs.h),
 * RS-485 disabled, and default (-1) RX/TX/DE/RE pins.
 */
struct SerialConfigs {
    uint32_t baudRate;      ///< One of the rates accepted by isValidBaudRate()
    uint8_t  dataBits;      ///< 5, 6, 7, or 8
    uint8_t  parity;        ///< 0=None, 1=Odd, 2=Even
    uint8_t  stopBits;      ///< 1 or 2
    bool     enabled;       ///< Whether applySerialConfig() should start this port
    bool     rxEnabled;     ///< Note: only enforced when a custom rxPin is set
    bool     txEnabled;     ///< Note: only enforced when a custom txPin is set
    int8_t   rxPin;         ///< -1 = platform default
    int8_t   txPin;         ///< -1 = platform default

    // RS-485 support
    int8_t   dePin;         ///< -1 = not used (Driver Enable)
    int8_t   rePin;         ///< -1 = not used (Receiver Enable)
    uint8_t  rs485Mode;     ///< SCM_RS485_NONE / _HALF_DUPLEX / _FULL_DUPLEX
    bool     rs485Invert;   ///< true = active-low DE/RE logic

    SerialConfigs();
};


/**
 * @brief Manages configuration, hardware application, RS-485 direction
 *        control, and persistence for both onboard UARTs.
 *
 * Typical usage:
 * @code
 * SerialConfigManager scm;
 * scm.begin(true, true);
 * scm.setSerialBaudRate(SER0, 9600);
 * scm.applySerialConfig(SER0);
 * scm.saveConfig();
 * @endcode
 */
class SerialConfigManager {
public:
    SerialConfigManager();
    ~SerialConfigManager();

    /**
     * @brief Initialize the manager and load any saved configuration.
     * @param formatOnFail Format the filesystem if mounting fails (FS backend only,
     *                     and only relevant when initStorage is true).
     * @param initStorage  Whether begin() itself should perform the storage
     *                     backend's setup (SCM_FS.begin() / EEPROM.begin()).
     *                     Pass true if this is the only place your sketch
     *                     initializes the filesystem/EEPROM. Pass false if
     *                     you already called SCM_FS.begin() / EEPROM.begin()
     *                     yourself before calling begin() - it will then
     *                     skip re-initializing storage and trust that it's
     *                     ready, going straight to loading any saved config
     *                     from it. This is NOT an "in-memory only" mode:
     *                     with false and a backend that was never actually
     *                     initialized (by you or by begin()), behavior is
     *                     undefined - only pass false when you're certain
     *                     the backend is already set up.
     * @return Always true today; use getLastError() to check for non-fatal
     *         storage warnings (e.g. "FS unavailable; settings will not persist").
     */
    bool begin(bool formatOnFail = false, bool initStorage = false);

    /** @brief Marks the manager as no longer initialized. Does not touch hardware UARTs. */
    void end();

    /** @brief Whether begin() has been called and completed. */
    bool isInitialized() const { return _initialized; }

    // ============================================================
    // Port-based unified API (port = SER0 or SER1)
    // ============================================================

    /** @brief Replace the full configuration for @p port. Validated before being applied. */
    bool setSerialConfig(uint8_t port, const SerialConfigs& config);

    /** @brief Get a copy of the current (in-memory) configuration for @p port. */
    SerialConfigs getSerialConfig(uint8_t port) const;

    /** @brief Apply the stored configuration for @p port to the hardware UART (restarts it). */
    bool applySerialConfig(uint8_t port);
    
    // Individual Setters
    bool setSerialBaudRate(uint8_t port, uint32_t baud);
    bool setSerialDataBits(uint8_t port, uint8_t bits);
    bool setSerialParity(uint8_t port, uint8_t parity);
    bool setSerialStopBits(uint8_t port, uint8_t bits);
    bool setSerialPins(uint8_t port, int8_t rxPin, int8_t txPin);
    bool enableSerial(uint8_t port, bool enable);
    bool setSerialRxTx(uint8_t port, bool rxEnable, bool txEnable);

    // ============================================================
    // Getter functions for current port settings
    // ============================================================
    uint32_t getBaudRate(uint8_t port) const;
    uint8_t getDataBits(uint8_t port) const;
    uint8_t getParity(uint8_t port) const;
    uint8_t getStopBits(uint8_t port) const;
    bool isEnabled(uint8_t port) const;
    bool isRxEnabled(uint8_t port) const;
    bool isTxEnabled(uint8_t port) const;
    int8_t getRxPin(uint8_t port) const;
    int8_t getTxPin(uint8_t port) const;
    int8_t getDePin(uint8_t port) const;
    int8_t getRePin(uint8_t port) const;
    uint8_t getRS485Mode(uint8_t port) const;
    bool isRS485Inverted(uint8_t port) const;

    /** @brief True if rs485Mode != SCM_RS485_NONE for @p port. */
    bool isRS485Enabled(uint8_t port) const;

    /** @brief Current RS-485 direction latch: true = transmitting, false = receiving. */
    bool isTransmitMode(uint8_t port) const;

    /** @brief True only if @p port is enabled AND applySerialConfig() has actually run for it. */
    bool isSerialActive(uint8_t port) const;

    // ============================================================
    // RS-485 Direction Control
    // ============================================================
    bool setRS485Pins(uint8_t port, int8_t dePin, int8_t rePin);
    bool setRS485Mode(uint8_t port, uint8_t mode);
    bool setRS485Invert(uint8_t port, bool invert);

    /** @brief Convenience toggle: true sets SCM_RS485_HALF_DUPLEX, false sets SCM_RS485_NONE. */
    bool enableRS485(uint8_t port, bool enable);

    /**
     * @brief Universal send helper - the recommended way to transmit data.
     *
     * The caller never needs to know or check whether @p port is wired for
     * RS-485: if RS-485 is enabled (setRS485Mode()/enableRS485()), this
     * automatically switches DE/RE to transmit, writes, waits for the data
     * to actually finish transmitting, then switches back to receive - all
     * transparently. For a plain port (or full-duplex RS-485, where DE/RE
     * never need to move), none of that direction/drain handling happens
     * at all, so the call returns as soon as the data is queued - as fast
     * as a raw HardwareSerial::write().
     *
     * The `const char*` overload measures the length itself via strlen(),
     * so you never need to pass one for a null-terminated string.
     *
     * Same signatures and behavior as writeRS485(), which forwards to
     * this function; use whichever name reads better at the call site.
     *
     * @param timeoutMs Max time to wait for TX buffer space while queueing the
     *                  data, in ms (0 = wait forever). Note: the final wait for
     *                  half-duplex RS-485 transmission to physically finish
     *                  uses HardwareSerial::flush(), which has no timeout of
     *                  its own - on a genuinely stuck UART that final step
     *                  could still block past timeoutMs.
     * @return false on timeout or write error.
     */
    bool writeSerial(uint8_t port, const uint8_t* data, size_t len, unsigned long timeoutMs = 1000);
    bool writeSerial(uint8_t port, const String& data, unsigned long timeoutMs = 1000);
    bool writeSerial(uint8_t port, const char* data, unsigned long timeoutMs = 1000);
    bool writeSerial(uint8_t port, char c, unsigned long timeoutMs = 1000);

    /** @brief Alias of writeSerial() (identical behavior), for call sites that want to name RS-485 intent. */
    bool writeRS485(uint8_t port, const uint8_t* data, size_t len, unsigned long timeoutMs = 1000) { return writeSerial(port, data, len, timeoutMs); }
    bool writeRS485(uint8_t port, const String& data, unsigned long timeoutMs = 1000)              { return writeSerial(port, data, timeoutMs); }
    bool writeRS485(uint8_t port, const char* data, unsigned long timeoutMs = 1000)                { return writeSerial(port, data, timeoutMs); }
    bool writeRS485(uint8_t port, char c, unsigned long timeoutMs = 1000)                          { return writeSerial(port, c, timeoutMs); }

    /** @brief Manually drive DE/RE into transmit state (half-duplex only; no-op otherwise). */
    void setTransmitMode(uint8_t port);
    /** @brief Manually drive DE/RE into receive state (half-duplex only; no-op otherwise). */
    void setReceiveMode(uint8_t port);

    // ============================================================
    // Save & Load Configuration
    // ============================================================

    /** @brief Persist the current in-memory configuration to the active storage backend. */
    bool saveConfig();

    /** @brief Load configuration from the active storage backend into memory (does not apply it). */
    bool loadConfig();

    /** @brief Reset both ports to defaults. Optionally persists via saveConfig(). */
    bool resetToDefaults(bool save = false);

    /**
     * @brief Whether a saved configuration currently exists in the active storage
     *        backend (config file for FS, valid signature for EEPROM). Always
     *        false when SCM_STORAGE_TYPE is SCM_STORAGE_NONE.
     */
    bool configFileExists() const;

    /** @brief Erase the saved configuration from the active storage backend, if any. */
    bool deleteConfigFile();

    /** @brief Export the current in-memory configuration as a JSON string (no I/O). */
    String configToJson() const;

    /**
     * @brief Import configuration from a JSON string produced by configToJson()
     *        (in-memory only - call saveConfig() afterwards to persist it).
     *        Rejects the update (returns false, keeps prior config) if any field
     *        fails validation, so a corrupted/malicious payload can never reach
     *        the hardware via applySerialConfig().
     */
    bool parseJsonConfig(const String& json);

    // Validation
    static bool isValidBaudRate(uint32_t baud);
    static bool isValidDataBits(uint8_t bits);
    static bool isValidParity(uint8_t parity);
    static bool isValidStopBits(uint8_t bits);
    static bool isValidPort(uint8_t port);
    static bool isValidRS485Mode(uint8_t mode);

    // Debug
    void printConfig(Stream& output = Serial);
    void printConfig(uint8_t port, Stream& output = Serial);
    String getLastError() const { return _lastError; }

    /** @brief Library release version string (see SCM_LIB_VERSION in SerialCfgDefs.h). */
    static const char* getVersion() { return SCM_LIB_VERSION; }

private:
    SerialConfigs _serial0Config;
    SerialConfigs _serial1Config;

    bool _initialized;
    String _lastError;

    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        bool _fsAvailable;
    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        bool _eepromInitialized;
    #endif

    // RS-485 state tracking
    bool _rs485TxMode[2];  // true = transmitting, false = receiving

    // Tracks whether HardwareSerial::begin() has actually been called
    // for each port (used by isSerialActive()).
    bool _serialApplied[2];

    // Storage System
    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        bool initFileSystem(bool formatOnFail);
        // Internal, FS-specific existence check. Named separately from the
        // public configFileExists() so the public method can dispatch across
        // storage backends without an ODR clash.
        bool fsConfigFileExists() const;

        bool saveToFs();
        bool loadFromFs();
    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        bool initEeprom();
        
        bool saveToEeprom();
        bool loadFromEeprom();
    #endif

    // Serialization
    String serializeConfig() const;
    bool deserializeConfig(const String& json);
    bool parseSerialConfig(const String& json, const char* key, SerialConfigs& config);
    // Helper to extract a value from a JSON fragment
    String extractJsonValue(const String& json, const char* key) const;

    // Error Handling
    void setError(const String& error);
    void clearError();

    // Hardware Application Helpers
    uint32_t getSerialConfigWord(const SerialConfigs& config) const;
    SerialConfigs* getConfigPtr(uint8_t port);
    const SerialConfigs* getConfigPtr(uint8_t port) const;
    HardwareSerial* getSerialPtr(uint8_t port);
    const HardwareSerial* getSerialPtr(uint8_t port) const;

    // RS-485 helpers
    void initRS485Pins(uint8_t port);
    void setRS485PinsState(uint8_t port, bool transmit);
};

#endif // SERIAL_CONFIG_MANAGER_H
