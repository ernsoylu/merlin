#ifndef DISPLAY_DEMO_H
#define DISPLAY_DEMO_H

#include "Rte_Type.h"

typedef struct {
    Rte_MonochromeFrameType frame;
} DisplayDemo_CtxType;

void DisplayDemo_Init(DisplayDemo_CtxType *context);
void DisplayDemo_Run(DisplayDemo_CtxType *context);
const Rte_MonochromeFrameType *DisplayDemo_GetFrame(
    const DisplayDemo_CtxType *context);

#endif
