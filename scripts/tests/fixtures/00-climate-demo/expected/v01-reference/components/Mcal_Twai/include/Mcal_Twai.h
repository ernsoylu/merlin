#ifndef MCAL_TWAI_H
#define MCAL_TWAI_H

#include <stdint.h>

#include "Std_Types.h"

#define MCAL_TWAI_QUEUE_CAPACITY 4U
#define MCAL_TWAI_MAX_DATA_LENGTH 8U
#define MCAL_TWAI_FLAG_EXTENDED 0x01U
#define MCAL_TWAI_FLAG_RTR 0x02U

typedef enum {
    MCAL_TWAI_STOPPED = 0,
    MCAL_TWAI_RUNNING,
    MCAL_TWAI_BUS_OFF,
    MCAL_TWAI_RECOVERING
} Mcal_TwaiStateType;

typedef struct {
    uint32_t identifier;
    uint8_t data[MCAL_TWAI_MAX_DATA_LENGTH];
    uint8_t length;
    uint8_t flags;
} Mcal_TwaiFrameType;

typedef Mcal_ResultType (*Mcal_TwaiTransmitFn)(void *context,
                                               const Mcal_TwaiFrameType *frame);
typedef Mcal_ResultType (*Mcal_TwaiReceiveFn)(void *context,
                                              Mcal_TwaiFrameType *frame);
typedef Mcal_ResultType (*Mcal_TwaiRecoverFn)(void *context);

typedef struct {
    void *context;
    Mcal_TwaiTransmitFn transmit;
    Mcal_TwaiReceiveFn receive;
    Mcal_TwaiRecoverFn recover;
} Mcal_TwaiInterfaceType;

typedef struct {
    Mcal_TwaiInterfaceType interface;
    Mcal_TwaiFrameType txQueue[MCAL_TWAI_QUEUE_CAPACITY];
    Mcal_TwaiFrameType rxQueue[MCAL_TWAI_QUEUE_CAPACITY];
    uint8_t txHead;
    uint8_t txTail;
    uint8_t txCount;
    uint8_t rxHead;
    uint8_t rxTail;
    uint8_t rxCount;
    uint16_t rxDropped;
    Mcal_TwaiStateType state;
} Mcal_TwaiHandleType;

Mcal_ResultType Mcal_Twai_ValidateFrame(const Mcal_TwaiFrameType *frame);
Mcal_ResultType Mcal_Twai_Init(Mcal_TwaiHandleType *handle,
                               Mcal_TwaiInterfaceType interface);
Mcal_ResultType Mcal_Twai_Start(Mcal_TwaiHandleType *handle);
Mcal_ResultType Mcal_Twai_Stop(Mcal_TwaiHandleType *handle);
Mcal_ResultType Mcal_Twai_Transmit(Mcal_TwaiHandleType *handle,
                                   const Mcal_TwaiFrameType *frame);
Mcal_ResultType Mcal_Twai_Receive(Mcal_TwaiHandleType *handle,
                                  Mcal_TwaiFrameType *frame);
Mcal_ResultType Mcal_Twai_Service(Mcal_TwaiHandleType *handle,
                                  uint8_t budget);
Mcal_ResultType Mcal_Twai_ReportState(Mcal_TwaiHandleType *handle,
                                      Mcal_TwaiStateType state);
Mcal_ResultType Mcal_Twai_Recover(Mcal_TwaiHandleType *handle);
uint8_t Mcal_Twai_PendingTx(const Mcal_TwaiHandleType *handle);
uint8_t Mcal_Twai_PendingRx(const Mcal_TwaiHandleType *handle);
uint16_t Mcal_Twai_DroppedRx(const Mcal_TwaiHandleType *handle);

#endif
