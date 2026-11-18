/**
 * @file SerialCfgDefs.h
 * @brief Unified configuration and persistence manager for the two
 *        hardware UARTs (Serial / Serial1) available on ESP32 & ESP8266,
 *        with optional RS-485 half/full-duplex direction control.
 */

#ifndef SERIAL_CFG_DEFS_H
#define SERIAL_CFG_DEFS_H

// Library / config-schema versioning
// SCM_LIB_VERSION    - human-readable release version (see CHANGELOG.md)
// SCM_CONFIG_VERSION - bump this whenever the on-disk/EEPROM layout of
//                      SerialConfigs (or the JSON schema) changes in a
//                      way old files/blobs can't be safely reinterpreted.
#define SCM_LIB_VERSION         "2.5.0"
#define SCM_CONFIG_VERSION      1

// filesystem selection (override with -DSCM_FS_MODE=... or #define before including)
//    0: SPIFFS
//    1: LittleFS  (default)
//    2: FFat      (ESP32 only)
#ifndef SCM_FS_MODE
    #define SCM_FS_MODE         1
#endif

// storage type Selection
#ifndef SCM_STORAGE_TYPE
    #define SCM_STORAGE_TYPE 		SCM_STORAGE_FS
#endif

// EEPROM
#define SCM_EEPROM_SIZE					512
#define SCM_EEPROM_OFST					100

// 1-byte signature written before the stored struct data so loadFromEeprom()
// can reliably tell "never saved" apart from "valid data that happens to
// start with 0xFF" (which the old baud-rate-byte check could not).
#define SCM_EEPROM_MAGIC				0xA5

// configuration
#define SCM_CONFIG_FILE         "/serial_config.json"
#define SCM_DEFAULT_BAUD        115200
#define SCM_DEFAULT_DATA_BITS   8
#define SCM_DEFAULT_PARITY      0   // 0=None, 1=Odd, 2=Even
#define SCM_DEFAULT_STOP_BITS   1
#define SCM_MAX_BAUD_RATE       2000000

// RS-485
#define SCM_RS485_TRM_DELAY()		delayMicroseconds(10)
#define SCM_RS485_RCV_DELAY()		delayMicroseconds(10)


// ============================================================
// Configuration Constants
// ============================================================
// Storage Types
#define SCM_STORAGE_NONE       	0
#define SCM_STORAGE_EEPROM     	1
#define SCM_STORAGE_FS         	2

// Serial numbers
#define SER0                    0   // Serial0
#define SER1                    1   // Serial1

// RS-485 Direction Modes
#define SCM_RS485_NONE          0
#define SCM_RS485_HALF_DUPLEX   1
#define SCM_RS485_FULL_DUPLEX   2

#endif // SERIAL_CFG_DEFS_H
