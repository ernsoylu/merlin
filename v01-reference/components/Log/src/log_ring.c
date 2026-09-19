#include "Log_Ring.h"

void Log_RingInit(Log_RingType *ring)
{
    *ring = (Log_RingType){0};
}

int Log_TryPush(Log_RingType *ring, Log_RecordType record)
{
    if (ring->count == LOG_RING_CAPACITY) {
        ring->overflowCount++;
        return 0;
    }
    ring->records[ring->head] = record;
    ring->head = (uint16_t)((ring->head + 1U) % LOG_RING_CAPACITY);
    ring->count++;
    return 1;
}

int Log_TryPop(Log_RingType *ring, Log_RecordType *record)
{
    if (ring->count == 0U || record == 0) {
        return 0;
    }
    *record = ring->records[ring->tail];
    ring->tail = (uint16_t)((ring->tail + 1U) % LOG_RING_CAPACITY);
    ring->count--;
    return 1;
}
