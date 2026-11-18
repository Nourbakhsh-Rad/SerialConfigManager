/**
 * @file SerialConfigManager.cpp
 * @brief Implementation of SerialConfigManager (see SerialConfigManager.h).
 */

#include "SerialConfigManager.h"
#include <string.h>

// On ESP32 variants built with "USB CDC On Boot" enabled, `Serial` is the USB
// CDC object (not a HardwareSerial), so SER0 maps to the real UART0 (`Serial0`).
#if defined(ESP32) && defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT)
    #define SCM_UART0   Serial0
#else
    #define SCM_UART0   Serial
#endif


#if (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
    #include <EEPROM.h>   // was missing entirely - EEPROM storage could not compile without it

    // Make sure the reserved EEPROM window is actually big enough for a
    // signature byte + version byte + two SerialConfigs structs. Was
    // previously a bare "uint8_t buffer[256]" with no relation to
    // SCM_EEPROM_SIZE at all.
    static_assert((SCM_EEPROM_OFST + 2 + 2 * sizeof(SerialConfigs)) <= SCM_EEPROM_SIZE,
                  "SCM_EEPROM_SIZE is too small for SCM_EEPROM_OFST + signature/version + 2x SerialConfigs");
#endif

// ============================================================
// SerialConfig Constructor
// ============================================================
SerialConfigs::SerialConfigs() :
    baudRate(SCM_DEFAULT_BAUD),
    dataBits(SCM_DEFAULT_DATA_BITS),
    parity(SCM_DEFAULT_PARITY),
    stopBits(SCM_DEFAULT_STOP_BITS),
    enabled(true),
    rxEnabled(true),
    txEnabled(true),
    rxPin(-1),
    txPin(-1),
    dePin(-1),
    rePin(-1),
    rs485Mode(SCM_RS485_NONE),
    rs485Invert(false) {}

// ============================================================
// SerialConfigManager Constructor / Destructor
// ============================================================
SerialConfigManager::SerialConfigManager()
    : _initialized(false), _lastError("") {
    _serial0Config = SerialConfigs();
    _serial1Config = SerialConfigs();
    _serial1Config.baudRate = 9600;
    _rs485TxMode[0] = false;
    _rs485TxMode[1] = false;
    _serialApplied[0] = false;
    _serialApplied[1] = false;
}

SerialConfigManager::~SerialConfigManager() {
    end();
}

// ============================================================
// Initialization
// ============================================================
bool SerialConfigManager::begin(bool formatOnFail, bool initStorage) {
    clearError();

    _initialized = true;

    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        _fsAvailable = true;

        if (initStorage) {
            _fsAvailable = initFileSystem(formatOnFail);
            if (!_fsAvailable) {
                setError("FS unavailable; settings will not persist");
            }
        }

        if (_fsAvailable) {
            if (fsConfigFileExists()) {
                if (!loadFromFs()) {
                    // resetToDefaults(true) -> saveConfig() clears _lastError,
                    // so remember the load failure and restore it afterwards.
                    String loadErr = _lastError.length() ? _lastError : String("Failed to load from FS");
                    resetToDefaults(true);
                    setError(loadErr + "; using defaults");
                }
            } else {
                resetToDefaults(true);
            }
        }

    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        (void)formatOnFail;
        _eepromInitialized = true;

        if (initStorage) {
            _eepromInitialized = initEeprom();
            if (!_eepromInitialized) {
                setError("EEPROM unavailable; settings will not persist");
            }
        }

        if (_eepromInitialized) {
            if (!loadFromEeprom()) {
                // First boot (no signature) is not an error; a version
                // mismatch already set a specific message - keep it.
                String loadErr = _lastError;
                resetToDefaults(true);
                if (loadErr.length()) setError(loadErr + "; using defaults");
            }
        }

    #else
        (void)formatOnFail;
        (void)initStorage;
        resetToDefaults(false);

    #endif

    return true;
}

void SerialConfigManager::end() {
    _initialized = false;
}

// ============================================================
// Internal Helpers
// ============================================================
SerialConfigs* SerialConfigManager::getConfigPtr(uint8_t port) {
    if (port == SER0) return &_serial0Config;
    if (port == SER1) return &_serial1Config;
    return nullptr;
}

const SerialConfigs* SerialConfigManager::getConfigPtr(uint8_t port) const {
    if (port == SER0) return &_serial0Config;
    if (port == SER1) return &_serial1Config;
    return nullptr;
}

HardwareSerial* SerialConfigManager::getSerialPtr(uint8_t port) {
    if (port == SER0) return &SCM_UART0;
    if (port == SER1) return &Serial1;
    return nullptr;
}

const HardwareSerial* SerialConfigManager::getSerialPtr(uint8_t port) const {
    if (port == SER0) return &SCM_UART0;
    if (port == SER1) return &Serial1;
    return nullptr;
}

// ============================================================
// Unified Port-based API
// ============================================================
bool SerialConfigManager::setSerialConfig(uint8_t port, const SerialConfigs& config) {
    clearError();
    if (!isValidPort(port)) {
        setError("Invalid port number. Use SER0 or SER1");
        return false;
    }
    if (!isValidBaudRate(config.baudRate)) {
        setError("Invalid baud rate");
        return false;
    }
    if (!isValidDataBits(config.dataBits)) {
        setError("Invalid data bits");
        return false;
    }
    if (!isValidParity(config.parity)) {
        setError("Invalid parity");
        return false;
    }
    if (!isValidStopBits(config.stopBits)) {
        setError("Invalid stop bits");
        return false;
    }

    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;

    *cfg = config;
    return true;
}

SerialConfigs SerialConfigManager::getSerialConfig(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    if (cfg) return *cfg;
    return SerialConfigs();
}

bool SerialConfigManager::applySerialConfig(uint8_t port) {
    clearError();
    if (!isValidPort(port)) {
        setError("Invalid port number. Use SER0 or SER1");
        return false;
    }

    SerialConfigs* cfg = getConfigPtr(port);
    HardwareSerial* ser = getSerialPtr(port);
    if (!cfg || !ser) return false;

    if (!cfg->enabled) {
        ser->end();
        _serialApplied[port] = false;
        return true;
    }

    uint32_t configWord = getSerialConfigWord(*cfg);

    ser->end();

#if defined(ESP32)
    // -1 for either pin means "platform default" for that pin.
    if (cfg->rxPin >= 0 || cfg->txPin >= 0) {
        ser->begin(cfg->baudRate, configWord, cfg->rxPin, cfg->txPin);
    } else {
        ser->begin(cfg->baudRate, configWord);
    }

    // Best effort only (see README "Known Limitations"): floats a *custom* pin.
    // Default pins are left alone so the USB/console UART is not affected.
    if (cfg->rxPin >= 0 && !cfg->rxEnabled) pinMode(cfg->rxPin, INPUT);
    if (cfg->txPin >= 0 && !cfg->txEnabled) pinMode(cfg->txPin, INPUT);
#elif defined(ESP8266)
    // ESP8266: custom UART pins are not supported by this library
    // (Serial1 is TX-only on GPIO2), so the default pins are always used.
    ser->begin(cfg->baudRate, configWord);
#endif

    // Initialize RS-485 pins if configured
    initRS485Pins(port);
    setReceiveMode(port);

    _serialApplied[port] = true;
    return true;
}

bool SerialConfigManager::setSerialBaudRate(uint8_t port, uint32_t baud) {
    if (!isValidPort(port)) return false;
    if (!isValidBaudRate(baud)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->baudRate = baud;
    return true;
}

bool SerialConfigManager::setSerialDataBits(uint8_t port, uint8_t bits) {
    if (!isValidPort(port)) return false;
    if (!isValidDataBits(bits)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->dataBits = bits;
    return true;
}

bool SerialConfigManager::setSerialParity(uint8_t port, uint8_t parity) {
    if (!isValidPort(port)) return false;
    if (!isValidParity(parity)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->parity = parity;
    return true;
}

bool SerialConfigManager::setSerialStopBits(uint8_t port, uint8_t bits) {
    if (!isValidPort(port)) return false;
    if (!isValidStopBits(bits)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->stopBits = bits;
    return true;
}

bool SerialConfigManager::setSerialPins(uint8_t port, int8_t rxPin, int8_t txPin) {
    if (!isValidPort(port)) return false;
#if defined(ESP8266)
    if (rxPin >= 0 || txPin >= 0) {
        setError("ESP8266: custom UART pins are not supported (pass -1, -1)");
        return false;
    }
#endif
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->rxPin = rxPin;
    cfg->txPin = txPin;
    return true;
}

bool SerialConfigManager::enableSerial(uint8_t port, bool enable) {
    if (!isValidPort(port)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->enabled = enable;
    return true;
}

bool SerialConfigManager::setSerialRxTx(uint8_t port, bool rxEnable, bool txEnable) {
    if (!isValidPort(port)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->rxEnabled = rxEnable;
    cfg->txEnabled = txEnable;
    return true;
}

// ============================================================
// Getter implementations
// ============================================================
uint32_t SerialConfigManager::getBaudRate(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->baudRate : 0;
}

uint8_t SerialConfigManager::getDataBits(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->dataBits : 0;
}

uint8_t SerialConfigManager::getParity(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->parity : 0;
}

uint8_t SerialConfigManager::getStopBits(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->stopBits : 0;
}

bool SerialConfigManager::isEnabled(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->enabled : false;
}

bool SerialConfigManager::isRxEnabled(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->rxEnabled : false;
}

bool SerialConfigManager::isTxEnabled(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->txEnabled : false;
}

int8_t SerialConfigManager::getRxPin(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->rxPin : -1;
}

int8_t SerialConfigManager::getTxPin(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->txPin : -1;
}

int8_t SerialConfigManager::getDePin(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->dePin : -1;
}

int8_t SerialConfigManager::getRePin(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->rePin : -1;
}

uint8_t SerialConfigManager::getRS485Mode(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->rs485Mode : SCM_RS485_NONE;
}

bool SerialConfigManager::isRS485Inverted(uint8_t port) const {
    const SerialConfigs* cfg = getConfigPtr(port);
    return cfg ? cfg->rs485Invert : false;
}

bool SerialConfigManager::isRS485Enabled(uint8_t port) const {
    return getRS485Mode(port) != SCM_RS485_NONE;
}

bool SerialConfigManager::isTransmitMode(uint8_t port) const {
    if (!isValidPort(port)) return false;
    return _rs485TxMode[port];
}

bool SerialConfigManager::isSerialActive(uint8_t port) const {
    if (!isValidPort(port)) return false;
    const SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg || !cfg->enabled) return false;
    return _serialApplied[port];
}

// ============================================================
// RS-485 Direction Control
// ============================================================
bool SerialConfigManager::setRS485Pins(uint8_t port, int8_t dePin, int8_t rePin) {
    if (!isValidPort(port)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->dePin = dePin;
    cfg->rePin = rePin;
    return true;
}

bool SerialConfigManager::setRS485Mode(uint8_t port, uint8_t mode) {
    if (!isValidPort(port)) return false;
    if (!isValidRS485Mode(mode)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->rs485Mode = mode;
    return true;
}

bool SerialConfigManager::setRS485Invert(uint8_t port, bool invert) {
    if (!isValidPort(port)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    cfg->rs485Invert = invert;
    return true;
}

bool SerialConfigManager::enableRS485(uint8_t port, bool enable) {
    if (!isValidPort(port)) return false;
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return false;
    if (enable) {
        cfg->rs485Mode = SCM_RS485_HALF_DUPLEX;
    } else {
        cfg->rs485Mode = SCM_RS485_NONE;
    }
    initRS485Pins(port);
    setReceiveMode(port);
    return true;
}

void SerialConfigManager::initRS485Pins(uint8_t port) {
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return;

    if (cfg->rs485Mode == SCM_RS485_NONE) return;

    if (cfg->dePin >= 0) {
        pinMode(cfg->dePin, OUTPUT);
    }
    if (cfg->rePin >= 0) {
        pinMode(cfg->rePin, OUTPUT);
    }

    // For full-duplex, set pins to always active (DE=HIGH, RE=LOW)
    if (cfg->rs485Mode == SCM_RS485_FULL_DUPLEX) {
        bool high = cfg->rs485Invert ? LOW : HIGH;
        bool low  = cfg->rs485Invert ? HIGH : LOW;
        if (cfg->dePin >= 0) digitalWrite(cfg->dePin, high);
        if (cfg->rePin >= 0) digitalWrite(cfg->rePin, low);
    }
}

void SerialConfigManager::setRS485PinsState(uint8_t port, bool transmit) {
    SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return;
    if (cfg->rs485Mode == SCM_RS485_NONE) return;

    // In full-duplex mode, pins are already set and we do nothing
    if (cfg->rs485Mode == SCM_RS485_FULL_DUPLEX) return;

    bool high = cfg->rs485Invert ? LOW : HIGH;
    bool low  = cfg->rs485Invert ? HIGH : LOW;

    if (transmit) {
        if (cfg->dePin >= 0) digitalWrite(cfg->dePin, high);
        if (cfg->rePin >= 0) digitalWrite(cfg->rePin, high);
        _rs485TxMode[port] = true;
        SCM_RS485_TRM_DELAY();
    } else {
        if (cfg->dePin >= 0) digitalWrite(cfg->dePin, low);
        if (cfg->rePin >= 0) digitalWrite(cfg->rePin, low);
        _rs485TxMode[port] = false;
        SCM_RS485_RCV_DELAY();
    }
}

void SerialConfigManager::setTransmitMode(uint8_t port) {
    setRS485PinsState(port, true);
}

void SerialConfigManager::setReceiveMode(uint8_t port) {
    setRS485PinsState(port, false);
}

// ============================================================
// Serial print...
// ============================================================
bool SerialConfigManager::writeSerial(uint8_t port, const uint8_t* data, size_t len, unsigned long timeoutMs) {
    HardwareSerial* ser = getSerialPtr(port);
    if (!ser) return false;

    // Only half-duplex RS-485 actually needs the transmit -> drain -> receive
    // dance: full-duplex leaves DE/RE permanently asserted (setRS485PinsState()
    // no-ops for it), and a plain port has no direction pins to switch at all.
    // Skipping that dance entirely for those two cases - instead of calling
    // setTransmitMode()/setReceiveMode() and then blocking for a drain that
    // was never needed - is what actually makes this "fast": the call now
    // returns as soon as the data is handed to the driver, the same as a
    // raw HardwareSerial::write() would.
    const SerialConfigs* cfg = getConfigPtr(port);
    bool needsDirectionControl = cfg && (cfg->rs485Mode == SCM_RS485_HALF_DUPLEX);

    if (needsDirectionControl) setTransmitMode(port);

    size_t written = 0;
    unsigned long start = millis();
    while (written < len) {
        size_t avail = ser->availableForWrite();
        if (avail == 0) {
            if (timeoutMs > 0 && (millis() - start) >= timeoutMs) {
                if (needsDirectionControl) setReceiveMode(port);
                return false;
            }
            delay(1);
            continue;
        }
        size_t toWrite = (avail < (len - written)) ? avail : (len - written);
        size_t n = ser->write(data + written, toWrite);
        written += n;
        if (n == 0) {
            // error
            if (needsDirectionControl) setReceiveMode(port);
            return false;
        }
        // reset timeout after successful write
        start = millis();
    }

    if (needsDirectionControl) {
        // DE must stay asserted until the bytes have actually finished
        // shifting out on the wire, or the last byte(s) get cut off.
        // flush() is the correct, portable way to block for that; the
        // previous hand-rolled loop here checked
        // "while (ser->availableForWrite() > 0)", which is backwards -
        // availableForWrite() reports free *buffer* space, not pending
        // *transmission*, and is almost always > 0 right after a write.
        // That meant this loop essentially never exited on its own and
        // instead sat busy-waiting for the full timeoutMs on every single
        // call, then reported failure.
        ser->flush();
        setReceiveMode(port);
    }
    return true;
}

bool SerialConfigManager::writeSerial(uint8_t port, const String& data, unsigned long timeoutMs) {
    return writeSerial(port, (const uint8_t*)data.c_str(), data.length(), timeoutMs);
}

bool SerialConfigManager::writeSerial(uint8_t port, const char* data, unsigned long timeoutMs) {
    if (!data) return false;
    return writeSerial(port, (const uint8_t*)data, strlen(data), timeoutMs);
}


bool SerialConfigManager::writeSerial(uint8_t port, char c, unsigned long timeoutMs) {
    return writeSerial(port, (const uint8_t*)&c, 1, timeoutMs);
}


// ============================================================
// Save & Load Configuration
// ============================================================
#if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
    bool SerialConfigManager::initFileSystem(bool formatOnFail) {
        #if defined(ESP32)
            if (!SCM_FS.begin(formatOnFail)) {
                return false;
            }
        #elif defined(ESP8266)
            if (!SCM_FS.begin()) {
                if (formatOnFail) {
                    SCM_FS.format();
                    if (!SCM_FS.begin()) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        #endif
        
        return true;
    }

    bool SerialConfigManager::fsConfigFileExists() const {
        return SCM_FS.exists(SCM_CONFIG_FILE);
    }

    bool SerialConfigManager::saveToFs() {
        clearError();

        if (!_fsAvailable) return false;
    
        String json = serializeConfig();
    
        File file = SCM_FS.open(SCM_CONFIG_FILE, "w");
        if (!file) {
            setError("Failed to open config file for writing");
            return false;
        }
    
        size_t written = file.print(json);
        file.close();
    
        if (written != json.length()) {
            setError("Failed to write complete config file");
            return false;
        }
    
        return true;
    }

    bool SerialConfigManager::loadFromFs() {
        clearError();

        if (!_fsAvailable) return false;
    
        if (!fsConfigFileExists()) {
            setError("Config file does not exist");
            return false;
        }
    
        File file = SCM_FS.open(SCM_CONFIG_FILE, "r");
        if (!file) {
            setError("Failed to open config file for reading");
            return false;
        }
    
        String json = file.readString();
        file.close();
    
        if (!deserializeConfig(json)) {
            setError("Failed to parse config JSON");
            return false;
        }
    
        return true;
    }

#elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
    bool SerialConfigManager::initEeprom() {
        #if defined(ESP32)
            return EEPROM.begin(SCM_EEPROM_SIZE);
        #else
            EEPROM.begin(SCM_EEPROM_SIZE);
            return true;
        #endif
    }

    // Layout in EEPROM:
    //     [magic byte][serial0Config][serial1Config]
    //     The magic byte lets loadFromEeprom() reliably distinguish "never
    //     saved" from "valid data whose first byte happens to be 0xFF" - the
    //     previous check (EEPROM.read(offset) == 0xFF) could misfire since nothing
    //     guarantees a real baud-rate's low byte is never 0xFF.
    //     Layout: [magic byte][version byte][serial0Config][serial1Config]
    //     Unlike JSON, this is a raw struct copy, so a layout change between
    //     library versions must be rejected outright rather than partially
    //     reinterpreted - hence the strict (not lenient) version check below.
    bool SerialConfigManager::saveToEeprom() {
        if (!_eepromInitialized) return false;

        const size_t dataSize = 2 * sizeof(SerialConfigs);
        uint8_t buffer[dataSize];
        size_t offset = 0;
        memcpy(buffer + offset, &_serial0Config, sizeof(SerialConfigs));
        offset += sizeof(SerialConfigs);
        memcpy(buffer + offset, &_serial1Config, sizeof(SerialConfigs));

        EEPROM.write(SCM_EEPROM_OFST, SCM_EEPROM_MAGIC);
        EEPROM.write(SCM_EEPROM_OFST + 1, SCM_CONFIG_VERSION);
        for (size_t i = 0; i < dataSize; i++) {
            EEPROM.write(SCM_EEPROM_OFST + 2 + i, buffer[i]);
        }
        return EEPROM.commit();
    }

    bool SerialConfigManager::loadFromEeprom() {
        if (!_eepromInitialized) return false;
        if (EEPROM.read(SCM_EEPROM_OFST) != SCM_EEPROM_MAGIC) return false;
        if (EEPROM.read(SCM_EEPROM_OFST + 1) != SCM_CONFIG_VERSION) {
            setError("Stored EEPROM config was written by an incompatible library version");
            return false;
        }

        const size_t dataSize = 2 * sizeof(SerialConfigs);
        uint8_t buffer[dataSize];
        for (size_t i = 0; i < dataSize; i++) {
            buffer[i] = EEPROM.read(SCM_EEPROM_OFST + 2 + i);
        }
        size_t offset = 0;
        memcpy(&_serial0Config, buffer + offset, sizeof(SerialConfigs));
        offset += sizeof(SerialConfigs);
        memcpy(&_serial1Config, buffer + offset, sizeof(SerialConfigs));
        return true;
    }

#endif

bool SerialConfigManager::saveConfig() {
    clearError();
    if (!_initialized) {
        setError("Manager not initialized");
        return false;
    }

    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        return saveToFs();
    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        return saveToEeprom();
    #else
        setError("No storage selected");
        return false;
    #endif
}

bool SerialConfigManager::loadConfig() {
    clearError();
    if (!_initialized) {
        setError("Manager not initialized");
        return false;
    }

    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        return loadFromFs();
    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        return loadFromEeprom();
    #else
        setError("No storage selected");
        return false;
    #endif
}


bool SerialConfigManager::resetToDefaults(bool save) {
    _serial0Config = SerialConfigs();
    _serial1Config = SerialConfigs();
    _serial1Config.baudRate = 9600;
    
    if (save)    return saveConfig();
    return true;
}

// ============================================================
// Storage-backend-agnostic helpers
// ============================================================
bool SerialConfigManager::configFileExists() const {
    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        return _fsAvailable && fsConfigFileExists();
    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        return _eepromInitialized && (EEPROM.read(SCM_EEPROM_OFST) == SCM_EEPROM_MAGIC);
    #else
        return false;
    #endif
}

bool SerialConfigManager::deleteConfigFile() {
    clearError();
    #if (SCM_STORAGE_TYPE == SCM_STORAGE_FS)
        if (!_fsAvailable) {
            setError("Filesystem not available");
            return false;
        }
        if (!fsConfigFileExists()) return true; // nothing to delete
        if (!SCM_FS.remove(SCM_CONFIG_FILE)) {
            setError("Failed to delete config file");
            return false;
        }
        return true;
    #elif (SCM_STORAGE_TYPE == SCM_STORAGE_EEPROM)
        if (!_eepromInitialized) {
            setError("EEPROM not available");
            return false;
        }
        // Clearing the signature byte is enough to invalidate the stored
        // blob without needing to zero the whole config area.
        EEPROM.write(SCM_EEPROM_OFST, 0x00);
        if (!EEPROM.commit()) {
            setError("Failed to commit EEPROM erase");
            return false;
        }
        return true;
    #else
        setError("No storage selected");
        return false;
    #endif
}

String SerialConfigManager::configToJson() const {
    return serializeConfig();
}

bool SerialConfigManager::parseJsonConfig(const String& json) {
    clearError();
    if (!deserializeConfig(json)) {
        // deserializeConfig()/parseSerialConfig() already called setError()
        // with a specific reason (missing key, invalid field, ...).
        if (_lastError.length() == 0) {
            setError("Failed to parse config JSON");
        }
        return false;
    }
    return true;
}

// ============================================================
// JSON Serialization
// ============================================================
String SerialConfigManager::serializeConfig() const {
    String json;
    // Avoids repeated heap reallocations while the string grows below
    // (each config block is ~230 bytes) - matters on ESP8266 where the
    // Arduino String otherwise fragments the heap on every "+=" here.
    json.reserve(600);
    json = "{\n";
    // Schema version so a future library release can tell an old file
    // apart from its own format instead of guessing from field presence.
    json += "  \"configVersion\": " + String(SCM_CONFIG_VERSION) + ",\n";

    auto appendConfig = [&](const char* name, const SerialConfigs& cfg) {
        json += "  \"";
        json += name;
        json += "\": {\n";
        json += "    \"baudRate\": " + String(cfg.baudRate) + ",\n";
        json += "    \"dataBits\": " + String(cfg.dataBits) + ",\n";
        json += "    \"parity\": " + String(cfg.parity) + ",\n";
        json += "    \"stopBits\": " + String(cfg.stopBits) + ",\n";
        json += "    \"enabled\": " + String(cfg.enabled ? "true" : "false") + ",\n";
        json += "    \"rxEnabled\": " + String(cfg.rxEnabled ? "true" : "false") + ",\n";
        json += "    \"txEnabled\": " + String(cfg.txEnabled ? "true" : "false") + ",\n";
        json += "    \"rxPin\": " + String(cfg.rxPin) + ",\n";
        json += "    \"txPin\": " + String(cfg.txPin) + ",\n";
        json += "    \"dePin\": " + String(cfg.dePin) + ",\n";
        json += "    \"rePin\": " + String(cfg.rePin) + ",\n";
        json += "    \"rs485Mode\": " + String(cfg.rs485Mode) + ",\n";
        json += "    \"rs485Invert\": " + String(cfg.rs485Invert ? "true" : "false") + "\n";
        json += "  }";
    };

    appendConfig("serial0", _serial0Config);
    json += ",\n";
    appendConfig("serial1", _serial1Config);
    json += "\n}";

    return json;
}

bool SerialConfigManager::deserializeConfig(const String& json) {
    // Version check is intentionally lenient (JSON is self-describing, so
    // an older/newer file can usually still be parsed field-by-field) -
    // this only records a diagnostic, it never blocks the parse. Contrast
    // with the EEPROM path, which stores a raw struct and must reject a
    // version mismatch outright (see loadFromEeprom()).
    String verStr = extractJsonValue(json, "configVersion");
    if (verStr.length() > 0 && verStr.toInt() > SCM_CONFIG_VERSION) {
        setError("Config file was saved by a newer library version; some fields may be ignored");
    }

    SerialConfigs s0, s1;
    s1.baudRate = 9600;   // same factory default as resetToDefaults()

    if (!parseSerialConfig(json, "serial0", s0)) {
        return false;
    }
    if (!parseSerialConfig(json, "serial1", s1)) {
        return false;
    }

    _serial0Config = s0;
    _serial1Config = s1;
    return true;
}

// Helper to extract a value from a JSON fragment (robust to whitespace)
String SerialConfigManager::extractJsonValue(const String& json, const char* key) const {
    String searchKey = "\"";
    searchKey += key;
    searchKey += "\"";
    int pos = json.indexOf(searchKey);
    if (pos == -1) return "";

    int colon = json.indexOf(':', pos);
    if (colon == -1) return "";

    // json.length() returns unsigned int on real Arduino cores; cast once
    // here so the loop variables below can stay plain int without tripping
    // -Wsign-compare. Config JSON is always small (well under INT_MAX), so
    // this is safe.
    int len = (int)json.length();

    // Find the start of the value (skip whitespace)
    int start = colon + 1;
    while (start < len && (json[start] == ' ' || json[start] == '\t' || json[start] == '\n' || json[start] == '\r')) {
        start++;
    }
    if (start >= len) return "";

    // Determine end of value: comma or closing brace
    int end = start;
    int braceDepth = 0;
    bool inString = false;
    while (end < len) {
        char c = json[end];
        if (c == '"' && (end == start || json[end-1] != '\\')) {
            inString = !inString;
        } else if (!inString) {
            if (c == ',' || c == '}' || c == ']') {
                break;
            }
            if (c == '{' || c == '[') {
                braceDepth++;
            } else if (c == '}' || c == ']') {
                if (braceDepth > 0) braceDepth--;
                else break;
            }
        }
        end++;
    }

    String val = json.substring(start, end);
    val.trim();
    return val;
}

bool SerialConfigManager::parseSerialConfig(const String& json, const char* key, SerialConfigs& config) {
    // Find the object for the given key
    String searchKey = "\"";
    searchKey += key;
    searchKey += "\"";
    int start = json.indexOf(searchKey);
    if (start == -1) return false;

    int objStart = json.indexOf('{', start);
    if (objStart == -1) return false;

    // Extract the entire object as a string (simplified: find matching closing brace)
    int len = (int)json.length(); // see comment in extractJsonValue() above
    int braceCount = 0;
    int objEnd = -1;
    for (int i = objStart; i < len; i++) {
        if (json[i] == '{') braceCount++;
        else if (json[i] == '}') {
            braceCount--;
            if (braceCount == 0) {
                objEnd = i;
                break;
            }
        }
    }
    if (objEnd == -1) return false;

    String obj = json.substring(objStart + 1, objEnd);

    // Helper to get integer value
    auto getInt = [&](const char* field) -> int {
        String val = extractJsonValue(obj, field);
        if (val.length() == 0) return -9999;
        char* endptr;
        long result = strtol(val.c_str(), &endptr, 10);
        if (endptr == val.c_str()) return -9999; // conversion failed
        return (int)result;
    };

    // Helper to get boolean value
    auto getBool = [&](const char* field) -> int {
        String val = extractJsonValue(obj, field);
        if (val.length() == 0) return -1;
        val.toLowerCase();
        if (val == "true") return 1;
        if (val == "false") return 0;
        return -1;
    };

    // Every numeric field is range-checked before being accepted. Previously
    // any integer from the JSON (however corrupted, truncated, or hand-edited)
    // was written straight into the struct, which applySerialConfig() then
    // hands to the hardware UART / pinMode() unchecked. Now a field that
    // fails validation aborts the whole parse (returns false) instead of
    // silently producing a config that could hang or misconfigure a port.
    int v;

    v = getInt("baudRate");
    if (v != -9999) {
        if (!isValidBaudRate((uint32_t)v)) { setError("Config contains invalid baudRate"); return false; }
        config.baudRate = (uint32_t)v;
    }
    v = getInt("dataBits");
    if (v != -9999) {
        if (!isValidDataBits((uint8_t)v)) { setError("Config contains invalid dataBits"); return false; }
        config.dataBits = (uint8_t)v;
    }
    v = getInt("parity");
    if (v != -9999) {
        if (!isValidParity((uint8_t)v)) { setError("Config contains invalid parity"); return false; }
        config.parity = (uint8_t)v;
    }
    v = getInt("stopBits");
    if (v != -9999) {
        if (!isValidStopBits((uint8_t)v)) { setError("Config contains invalid stopBits"); return false; }
        config.stopBits = (uint8_t)v;
    }
    v = getInt("rs485Mode");
    if (v != -9999) {
        if (!isValidRS485Mode((uint8_t)v)) { setError("Config contains invalid rs485Mode"); return false; }
        config.rs485Mode = (uint8_t)v;
    }

    // Pin fields are int8_t; reject anything that wouldn't round-trip.
    auto getPin = [&](const char* field, int8_t& out) -> bool {
        int val = getInt(field);
        if (val == -9999) return true; // field absent, keep default
        if (val < -128 || val > 127) { setError(String("Config contains out-of-range ") + field); return false; }
        out = (int8_t)val;
        return true;
    };
    if (!getPin("rxPin", config.rxPin)) return false;
    if (!getPin("txPin", config.txPin)) return false;
    if (!getPin("dePin", config.dePin)) return false;
    if (!getPin("rePin", config.rePin)) return false;

    v = getBool("enabled");      if (v != -1) config.enabled = v;
    v = getBool("rxEnabled");    if (v != -1) config.rxEnabled = v;
    v = getBool("txEnabled");    if (v != -1) config.txEnabled = v;
    v = getBool("rs485Invert");  if (v != -1) config.rs485Invert = v;

    return true;
}

// ============================================================
// Validation Methods
// ============================================================
bool SerialConfigManager::isValidBaudRate(uint32_t baud) {
    const uint32_t validRates[] = {
        300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800,
        38400, 57600, 74880, 115200, 128000, 230400, 250000,
        256000, 460800, 500000, 576000, 921600, 1000000, 1500000, 2000000
    };
    for (size_t i = 0; i < sizeof(validRates)/sizeof(validRates[0]); i++) {
        if (baud == validRates[i]) return true;
    }
    return false;
}

bool SerialConfigManager::isValidDataBits(uint8_t bits) {
    return bits >= 5 && bits <= 8;
}

bool SerialConfigManager::isValidParity(uint8_t parity) {
    return parity <= 2;
}

bool SerialConfigManager::isValidStopBits(uint8_t bits) {
    return bits == 1 || bits == 2;
}

bool SerialConfigManager::isValidPort(uint8_t port) {
    return port <= 1;
}

bool SerialConfigManager::isValidRS485Mode(uint8_t mode) {
    return mode <= 2;
}

// ============================================================
// Hardware Config Word Helper
// ============================================================
uint32_t SerialConfigManager::getSerialConfigWord(const SerialConfigs& config) const {
    uint8_t db = config.dataBits;
    uint8_t par = config.parity;
    uint8_t sb = config.stopBits;

    // Define a macro to select based on parity and stop bits
#define SELECT_WORD(base5, base6, base7, base8) \
    (db == 5 ? base5 : db == 6 ? base6 : db == 7 ? base7 : base8)

    if (par == 0) { // None
        if (sb == 1)
            return SELECT_WORD(SERIAL_5N1, SERIAL_6N1, SERIAL_7N1, SERIAL_8N1);
        else // sb == 2
            return SELECT_WORD(SERIAL_5N2, SERIAL_6N2, SERIAL_7N2, SERIAL_8N2);
    } else if (par == 1) { // Odd
        if (sb == 1)
            return SELECT_WORD(SERIAL_5O1, SERIAL_6O1, SERIAL_7O1, SERIAL_8O1);
        else
            return SELECT_WORD(SERIAL_5O2, SERIAL_6O2, SERIAL_7O2, SERIAL_8O2);
    } else { // Even
        if (sb == 1)
            return SELECT_WORD(SERIAL_5E1, SERIAL_6E1, SERIAL_7E1, SERIAL_8E1);
        else
            return SELECT_WORD(SERIAL_5E2, SERIAL_6E2, SERIAL_7E2, SERIAL_8E2);
    }
#undef SELECT_WORD
}

// ============================================================
// Debug & Print
// ============================================================
void SerialConfigManager::printConfig(Stream& output) {
    output.println("========== Serial Configuration ==========");
    printConfig(0, output);
    printConfig(1, output);
    output.println("==========================================");
}

void SerialConfigManager::printConfig(uint8_t port, Stream& output) {
    const SerialConfigs* cfg = getConfigPtr(port);
    if (!cfg) return;

    output.print("--- Serial Port "); output.print(port); output.println(" ---");
    output.print("Baud Rate:   "); output.println(cfg->baudRate);
    output.print("Data Bits:   "); output.println(cfg->dataBits);
    output.print("Parity:      "); 
    output.println(cfg->parity == 0 ? "None" : cfg->parity == 1 ? "Odd" : "Even");
    output.print("Stop Bits:   "); output.println(cfg->stopBits);
    output.print("Enabled:     "); output.println(cfg->enabled ? "Yes" : "No");
    output.print("RX Enabled:  "); output.println(cfg->rxEnabled ? "Yes" : "No");
    output.print("TX Enabled:  "); output.println(cfg->txEnabled ? "Yes" : "No");
    output.print("RX Pin:      "); output.println(cfg->rxPin);
    output.print("TX Pin:      "); output.println(cfg->txPin);

    if (cfg->rs485Mode != SCM_RS485_NONE) {
        output.println("--- RS-485 ---");
        output.print("Mode:        ");
        output.println(cfg->rs485Mode == SCM_RS485_HALF_DUPLEX ? "Half-Duplex" : "Full-Duplex");
        output.print("DE Pin:      "); output.println(cfg->dePin);
        output.print("RE Pin:      "); output.println(cfg->rePin);
        output.print("Inverted:    "); output.println(cfg->rs485Invert ? "Yes" : "No");
    }
    output.println();
}

// ============================================================
// Error Handling
// ============================================================
void SerialConfigManager::setError(const String& error) {
    _lastError = error;
}

void SerialConfigManager::clearError() {
    _lastError = "";
}
