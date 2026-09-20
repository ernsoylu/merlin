#include "Log_Ring.h"

void Log_RingInit(Log_RingType *ring)
{
    ring->head = 0U;
    ring->tail = 0U;
    ring->count = 0U;
    ring->overflowCount = 0U;
#ifdef ESP_PLATFORM
    ring->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
#endif
}

int Log_TryPush(Log_RingType *ring, Log_RecordType record)
{
#ifdef ESP_PLATFORM
    portENTER_CRITICAL(&ring->lock);
#endif
    if (ring->count == LOG_RING_CAPACITY) {
        ring->overflowCount++;
#ifdef ESP_PLATFORM
        portEXIT_CRITICAL(&ring->lock);
#endif
        return 0;
    }
    ring->records[ring->head] = record;
    ring->head = (uint16_t)((ring->head + 1U) % LOG_RING_CAPACITY);
    ring->count++;
#ifdef ESP_PLATFORM
    portEXIT_CRITICAL(&ring->lock);
#endif
    return 1;
}

int Log_TryPop(Log_RingType *ring, Log_RecordType *record)
{
#ifdef ESP_PLATFORM
    portENTER_CRITICAL(&ring->lock);
#endif
    if (ring->count == 0U || record == 0) {
#ifdef ESP_PLATFORM
        portEXIT_CRITICAL(&ring->lock);
#endif
        return 0;
    }
    *record = ring->records[ring->tail];
    ring->tail = (uint16_t)((ring->tail + 1U) % LOG_RING_CAPACITY);
    ring->count--;
#ifdef ESP_PLATFORM
    portEXIT_CRITICAL(&ring->lock);
#endif
    return 1;
}
