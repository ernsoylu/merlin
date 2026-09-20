#ifndef NVM_H
#define NVM_H

#include <stdint.h>

#define NVM_BLOCK_MAX_BYTES 64U
#define NVM_RTF_CRC_INVALID 7U

typedef enum {
    NVM_OK = 0,
    NVM_INVALID,
    NVM_NOT_FOUND,
    NVM_CRC_INVALID,
    NVM_STORAGE_ERROR
} Nvm_ResultType;

typedef enum {
    NVM_QUALITY_INITIAL = 0,
    NVM_QUALITY_VALID
} Nvm_QualityType;

typedef Nvm_ResultType (*Nvm_BackendReadFn)(void *context, uint8_t *data,
                                            uint16_t capacity,
                                            uint16_t *length);
typedef Nvm_ResultType (*Nvm_BackendWriteFn)(void *context,
                                             const uint8_t *data,
                                             uint16_t length);
typedef void (*Nvm_WatchdogControlFn)(void *context, uint8_t enabled);

typedef struct {
    void *context;
    Nvm_BackendReadFn read;
    Nvm_BackendWriteFn write;
    Nvm_WatchdogControlFn watchdog;
} Nvm_BackendType;

typedef struct {
    uint16_t version;
    uint16_t length;
    uint8_t data[NVM_BLOCK_MAX_BYTES];
    uint8_t defaults[NVM_BLOCK_MAX_BYTES];
    uint8_t dirty;
    Nvm_QualityType quality;
} Nvm_BlockType;

Nvm_ResultType Nvm_BlockInit(Nvm_BlockType *block, uint16_t version,
                             const uint8_t *defaults, uint16_t length);
Nvm_ResultType Nvm_LoadBlock(Nvm_BlockType *block,
                             const Nvm_BackendType *backend);
Nvm_ResultType Nvm_WriteBlock(Nvm_BlockType *block, const uint8_t *data,
                              uint16_t length);
Nvm_ResultType Nvm_WriteAll(Nvm_BlockType *block,
                            const Nvm_BackendType *backend);

#ifdef ESP_PLATFORM
#include "nvs.h"

typedef struct {
    nvs_handle_t handle;
    const char *key;
} Nvm_NvsBackendType;

Nvm_ResultType Nvm_NvsOpen(Nvm_NvsBackendType *backend,
                           const char *nameSpace, const char *key);
void Nvm_NvsClose(Nvm_NvsBackendType *backend);
Nvm_BackendType Nvm_NvsGetBackend(Nvm_NvsBackendType *backend);
#endif

#endif
