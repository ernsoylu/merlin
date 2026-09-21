#include <assert.h>
#include <string.h>

#include "Nvm.h"

typedef struct {
    uint8_t record[sizeof(uint32_t) + sizeof(uint16_t) * 2U +
                   NVM_BLOCK_MAX_BYTES + sizeof(uint32_t)];
    uint16_t length;
    unsigned int watchdogCalls;
} StoreType;

static Nvm_ResultType store_read(void *context, uint8_t *data,
                                 uint16_t capacity, uint16_t *length)
{
    const StoreType *store = context;
    if (store->length == 0U) {
        return NVM_NOT_FOUND;
    }
    assert(store->length <= capacity);
    (void)memcpy(data, store->record, store->length);
    *length = store->length;
    return NVM_OK;
}

static Nvm_ResultType store_write(void *context, const uint8_t *data,
                                  uint16_t length)
{
    StoreType *store = context;
    assert(length <= sizeof(store->record));
    (void)memcpy(store->record, data, length);
    store->length = length;
    return NVM_OK;
}

static void watchdog(void *context, uint8_t enabled)
{
    StoreType *store = context;
    assert(enabled == 0U || enabled == 1U);
    store->watchdogCalls++;
}

int main(void)
{
    const uint8_t defaults[] = {1U, 2U, 3U};
    const uint8_t updated[] = {8U, 9U, 10U};
    Nvm_BlockType block;
    StoreType store = {0};
    const Nvm_BackendType backend = {
        .context = &store, .read = store_read, .write = store_write,
        .watchdog = watchdog
    };

    assert(Nvm_BlockInit(&block, 2U, defaults, sizeof(defaults)) == NVM_OK);
    assert(block.quality == NVM_QUALITY_INITIAL && block.dirty);
    assert(Nvm_LoadBlock(&block, &backend) == NVM_NOT_FOUND);
    assert(block.data[0] == 1U && block.quality == NVM_QUALITY_INITIAL);
    assert(Nvm_WriteBlock(&block, updated, sizeof(updated)) == NVM_OK);
    assert(Nvm_WriteAll(&block, &backend) == NVM_OK);
    assert(!block.dirty && store.watchdogCalls == 2U);
    assert(Nvm_LoadBlock(&block, &backend) == NVM_OK);
    assert(memcmp(block.data, updated, sizeof(updated)) == 0);

    store.record[sizeof(store.record) - 1U] ^= 0x01U;
    assert(Nvm_LoadBlock(&block, &backend) == NVM_CRC_INVALID);
    assert(block.data[0] == 1U && block.quality == NVM_QUALITY_INITIAL);
    assert(Nvm_BlockInit(&block, 2U, defaults, NVM_BLOCK_MAX_BYTES + 1U) ==
           NVM_INVALID);
    return 0;
}
