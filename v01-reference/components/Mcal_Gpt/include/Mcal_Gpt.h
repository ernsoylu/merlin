#ifndef MCAL_GPT_H
#define MCAL_GPT_H

#include <stdint.h>

#include "Std_Types.h"

/* Free-running target timebase used for bounded execution measurements. */
Mcal_ResultType Mcal_Gpt_GetTimeUs(int64_t *timeUs);

#endif
