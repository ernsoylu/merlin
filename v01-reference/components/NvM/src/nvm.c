#include "Nvm.h"

#include <string.h>

#define NVM_RECORD_MAGIC 0x4D4E564DU

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint8_t data[NVM_BLOCK_MAX_BYTES];
    uint32_t crc;
} Nvm_RecordType;

static uint32_t crc32(const uint8_t *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint16_t i = 0U; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static uint32_t record_crc(const Nvm_RecordType *record)
{
    return crc32((const uint8_t *)&record->version,
                 (uint16_t)(sizeof(record->version) +
                            sizeof(record->length) + record->length));
}

static void restore_defaults(Nvm_BlockType *block)
{
    (void)memcpy(block->data, block->defaults, block->length);
    block->dirty = 1U;
    block->quality = NVM_QUALITY_INITIAL;
}

Nvm_ResultType Nvm_BlockInit(Nvm_BlockType *block, uint16_t version,
                             const uint8_t *defaults, uint16_t length)
{
    if (block == 0 || defaults == 0 || length == 0U ||
        length > NVM_BLOCK_MAX_BYTES) {
        return NVM_INVALID;
    }
    *block = (Nvm_BlockType){.version = version, .length = length,
                             .quality = NVM_QUALITY_INITIAL};
    (void)memcpy(block->defaults, defaults, length);
    restore_defaults(block);
    return NVM_OK;
}

Nvm_ResultType Nvm_LoadBlock(Nvm_BlockType *block,
                             const Nvm_BackendType *backend)
{
    if (block == 0 || backend == 0 || backend->read == 0) {
        return NVM_INVALID;
    }
    Nvm_RecordType record = {0};
    uint16_t length = sizeof(record);
    Nvm_ResultType result = backend->read(backend->context,
                                          (uint8_t *)&record,
                                          sizeof(record), &length);
    if (result != NVM_OK) {
        restore_defaults(block);
        return result;
    }
    if (length != sizeof(record) || record.magic != NVM_RECORD_MAGIC ||
        record.version != block->version || record.length != block->length ||
        record.crc != record_crc(&record)) {
        restore_defaults(block);
        return NVM_CRC_INVALID;
    }
    (void)memcpy(block->data, record.data, block->length);
    block->dirty = 0U;
    block->quality = NVM_QUALITY_VALID;
    return NVM_OK;
}

Nvm_ResultType Nvm_WriteBlock(Nvm_BlockType *block, const uint8_t *data,
                              uint16_t length)
{
    if (block == 0 || data == 0 || length != block->length ||
        length == 0U || length > NVM_BLOCK_MAX_BYTES) {
        return NVM_INVALID;
    }
    (void)memcpy(block->data, data, length);
    block->dirty = 1U;
    block->quality = NVM_QUALITY_VALID;
    return NVM_OK;
}

Nvm_ResultType Nvm_WriteAll(Nvm_BlockType *block,
                            const Nvm_BackendType *backend)
{
    if (block == 0 || backend == 0 || backend->write == 0) {
        return NVM_INVALID;
    }
    if (block->dirty == 0U) {
        return NVM_OK;
    }
    Nvm_RecordType record = {
        .magic = NVM_RECORD_MAGIC,
        .version = block->version,
        .length = block->length
    };
    (void)memcpy(record.data, block->data, block->length);
    record.crc = record_crc(&record);
    if (backend->watchdog != 0) {
        backend->watchdog(backend->context, 0U);
    }
    Nvm_ResultType result = backend->write(backend->context,
                                           (const uint8_t *)&record,
                                           sizeof(record));
    if (backend->watchdog != 0) {
        backend->watchdog(backend->context, 1U);
    }
    if (result == NVM_OK) {
        block->dirty = 0U;
    }
    return result;
}

#ifdef ESP_PLATFORM

#include "esp_err.h"
#include "nvs_flash.h"

static Nvm_ResultType map_nvs_error(esp_err_t error)
{
    return error == ESP_OK ? NVM_OK :
           error == ESP_ERR_NVS_NOT_FOUND ? NVM_NOT_FOUND :
           NVM_STORAGE_ERROR;
}

static Nvm_ResultType nvs_read(void *context, uint8_t *data,
                               uint16_t capacity, uint16_t *length)
{
    Nvm_NvsBackendType *backend = context;
    size_t size = capacity;
    esp_err_t error = nvs_get_blob(backend->handle, backend->key, data, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return NVM_NOT_FOUND;
    }
    if (error != ESP_OK || size > UINT16_MAX) {
        return NVM_STORAGE_ERROR;
    }
    *length = (uint16_t)size;
    return NVM_OK;
}

static Nvm_ResultType nvs_write(void *context, const uint8_t *data,
                                uint16_t length)
{
    Nvm_NvsBackendType *backend = context;
    Nvm_ResultType result = map_nvs_error(nvs_set_blob(backend->handle,
                                                       backend->key,
                                                       data, length));
    return result == NVM_OK ? map_nvs_error(nvs_commit(backend->handle))
                            : result;
}

Nvm_ResultType Nvm_NvsOpen(Nvm_NvsBackendType *backend,
                           const char *nameSpace, const char *key)
{
    if (backend == 0 || nameSpace == 0 || key == 0) {
        return NVM_INVALID;
    }
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES ||
        error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) {
            return NVM_STORAGE_ERROR;
        }
        error = nvs_flash_init();
    }
    if (error != ESP_OK || nvs_open(nameSpace, NVS_READWRITE,
                                    &backend->handle) != ESP_OK) {
        return NVM_STORAGE_ERROR;
    }
    backend->key = key;
    return NVM_OK;
}

void Nvm_NvsClose(Nvm_NvsBackendType *backend)
{
    if (backend != 0) {
        nvs_close(backend->handle);
        backend->handle = 0;
        backend->key = 0;
    }
}

Nvm_BackendType Nvm_NvsGetBackend(Nvm_NvsBackendType *backend)
{
    return (Nvm_BackendType){
        .context = backend, .read = nvs_read, .write = nvs_write
    };
}

#endif
