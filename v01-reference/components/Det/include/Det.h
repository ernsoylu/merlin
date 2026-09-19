#ifndef DET_H
#define DET_H

#include <stdint.h>

#include "Log_Ring.h"

void Det_ReportRuntimeFault(Log_RingType *ring, uint32_t code,
                            uint32_t argument, int64_t timestampUs);

#endif
