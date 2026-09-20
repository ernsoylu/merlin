#include "Mcal_Twai.h"

static uint32_t max_identifier(uint8_t flags)
{
    return (flags & MCAL_TWAI_FLAG_EXTENDED) != 0U ? 0x1FFFFFFFU : 0x7FFU;
}

Mcal_ResultType Mcal_Twai_ValidateFrame(const Mcal_TwaiFrameType *frame)
{
    if (frame == 0 || frame->length > MCAL_TWAI_MAX_DATA_LENGTH ||
        (frame->flags & (uint8_t)~(MCAL_TWAI_FLAG_EXTENDED |
                                    MCAL_TWAI_FLAG_RTR)) != 0U ||
        frame->identifier > max_identifier(frame->flags)) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Init(Mcal_TwaiHandleType *handle,
                               Mcal_TwaiInterfaceType interface)
{
    if (handle == 0) {
        return MCAL_INVALID_ARG;
    }
    *handle = (Mcal_TwaiHandleType){
        .interface = interface,
        .state = MCAL_TWAI_STOPPED
    };
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Start(Mcal_TwaiHandleType *handle)
{
    if (handle == 0) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state != MCAL_TWAI_STOPPED) {
        return MCAL_BUSY;
    }
    handle->state = MCAL_TWAI_RUNNING;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Stop(Mcal_TwaiHandleType *handle)
{
    if (handle == 0) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state == MCAL_TWAI_STOPPED) {
        return MCAL_BUSY;
    }
    handle->state = MCAL_TWAI_STOPPED;
    handle->txHead = 0U;
    handle->txTail = 0U;
    handle->txCount = 0U;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Transmit(Mcal_TwaiHandleType *handle,
                                   const Mcal_TwaiFrameType *frame)
{
    if (handle == 0 || Mcal_Twai_ValidateFrame(frame) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state != MCAL_TWAI_RUNNING) {
        return handle->state == MCAL_TWAI_BUS_OFF ? MCAL_HW_FAIL : MCAL_BUSY;
    }
    if (handle->txCount == MCAL_TWAI_QUEUE_CAPACITY) {
        return MCAL_BUSY;
    }
    handle->txQueue[handle->txTail] = *frame;
    handle->txTail = (uint8_t)((handle->txTail + 1U) % MCAL_TWAI_QUEUE_CAPACITY);
    handle->txCount++;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Receive(Mcal_TwaiHandleType *handle,
                                  Mcal_TwaiFrameType *frame)
{
    if (handle == 0 || frame == 0) {
        return MCAL_INVALID_ARG;
    }
    if (handle->rxCount == 0U) {
        return MCAL_TIMEOUT;
    }
    *frame = handle->rxQueue[handle->rxHead];
    handle->rxHead = (uint8_t)((handle->rxHead + 1U) % MCAL_TWAI_QUEUE_CAPACITY);
    handle->rxCount--;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Service(Mcal_TwaiHandleType *handle,
                                  uint8_t budget)
{
    if (handle == 0 || budget == 0U) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state != MCAL_TWAI_RUNNING) {
        return MCAL_BUSY;
    }
    uint8_t serviced = 0U;
    for (uint8_t i = 0U; i < budget; ++i) {
        if (handle->txCount != 0U) {
            if (handle->interface.transmit == 0) {
                return MCAL_UNSUPPORTED;
            }
            Mcal_ResultType result = handle->interface.transmit(
                handle->interface.context, &handle->txQueue[handle->txHead]);
            if (result != MCAL_OK) {
                return result;
            }
            handle->txHead = (uint8_t)((handle->txHead + 1U) % MCAL_TWAI_QUEUE_CAPACITY);
            handle->txCount--;
            serviced++;
        }
        if (handle->interface.receive == 0) {
            continue;
        }
        Mcal_TwaiFrameType frame;
        const Mcal_ResultType result = handle->interface.receive(
            handle->interface.context, &frame);
        if (result == MCAL_TIMEOUT) {
            continue;
        }
        if (result != MCAL_OK) {
            return result;
        }
        if (Mcal_Twai_ValidateFrame(&frame) != MCAL_OK) {
            return MCAL_INVALID_ARG;
        }
        if (handle->rxCount == MCAL_TWAI_QUEUE_CAPACITY) {
            handle->rxDropped++;
            continue;
        }
        handle->rxQueue[handle->rxTail] = frame;
        handle->rxTail = (uint8_t)((handle->rxTail + 1U) % MCAL_TWAI_QUEUE_CAPACITY);
        handle->rxCount++;
        serviced++;
    }
    return serviced == 0U ? MCAL_BUSY : MCAL_OK;
}

Mcal_ResultType Mcal_Twai_ReportState(Mcal_TwaiHandleType *handle,
                                      Mcal_TwaiStateType state)
{
    if (handle == 0 || state > MCAL_TWAI_RECOVERING) {
        return MCAL_INVALID_ARG;
    }
    handle->state = state;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Twai_Recover(Mcal_TwaiHandleType *handle)
{
    if (handle == 0) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state != MCAL_TWAI_BUS_OFF) {
        return MCAL_BUSY;
    }
    if (handle->interface.recover == 0) {
        return MCAL_UNSUPPORTED;
    }
    handle->state = MCAL_TWAI_RECOVERING;
    const Mcal_ResultType result = handle->interface.recover(
        handle->interface.context);
    handle->state = result == MCAL_OK ? MCAL_TWAI_RUNNING : MCAL_TWAI_BUS_OFF;
    return result;
}

uint8_t Mcal_Twai_PendingTx(const Mcal_TwaiHandleType *handle)
{
    return handle == 0 ? 0U : handle->txCount;
}

uint8_t Mcal_Twai_PendingRx(const Mcal_TwaiHandleType *handle)
{
    return handle == 0 ? 0U : handle->rxCount;
}

uint16_t Mcal_Twai_DroppedRx(const Mcal_TwaiHandleType *handle)
{
    return handle == 0 ? 0U : handle->rxDropped;
}
