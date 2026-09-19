#include <assert.h>

#include "Det.h"

int main(void)
{
    Log_RingType ring;
    Log_RingInit(&ring);
    Det_ReportRuntimeFault(&ring, 2U, 76U, 1000);
    Log_RecordType record;
    assert(Log_TryPop(&ring, &record));
    assert(record.code == 2U && record.argument == 76U && record.timestampUs == 1000);
    for (unsigned int i = 0; i < LOG_RING_CAPACITY; ++i) {
        assert(Log_TryPush(&ring, (Log_RecordType){.code = i}));
    }
    assert(!Log_TryPush(&ring, (Log_RecordType){0}));
    assert(ring.overflowCount == 1U);
    return 0;
}
