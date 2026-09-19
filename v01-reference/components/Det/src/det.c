#include "Det.h"

void Det_ReportRuntimeFault(Log_RingType *ring, uint32_t code,
                            uint32_t argument, int64_t timestampUs)
{
    (void)Log_TryPush(ring, (Log_RecordType){
        .code = code, .argument = argument, .timestampUs = timestampUs
    });
}
