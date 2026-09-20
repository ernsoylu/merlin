#ifndef LOG_RING_H
#define LOG_RING_H

#include <stdint.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#endif

#define LOG_RING_CAPACITY 16U

typedef struct {
    uint32_t code;
    uint32_t argument;
    int64_t timestampUs;
    const char *label;
    const void *payload;
} Log_RecordType;

typedef struct {
    Log_RecordType records[LOG_RING_CAPACITY];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t overflowCount;
#ifdef ESP_PLATFORM
    /* ponytail: one short critical section; split rings only if contention measures badly. */
    portMUX_TYPE lock;
#endif
} Log_RingType;

void Log_RingInit(Log_RingType *ring);
int Log_TryPush(Log_RingType *ring, Log_RecordType record);
int Log_TryPop(Log_RingType *ring, Log_RecordType *record);

#endif
