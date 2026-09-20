#include <assert.h>

#include "Mcal_Twai.h"

typedef struct {
    Mcal_TwaiFrameType received;
    unsigned int transmitted;
    unsigned int recoveries;
    Mcal_ResultType recoveryResult;
} MockType;

static Mcal_ResultType transmit(void *context, const Mcal_TwaiFrameType *frame)
{
    MockType *mock = context;
    mock->received = *frame;
    mock->transmitted++;
    return MCAL_OK;
}

static Mcal_ResultType receive(void *context, Mcal_TwaiFrameType *frame)
{
    MockType *mock = context;
    if (mock->received.identifier == 0U) {
        return MCAL_TIMEOUT;
    }
    *frame = mock->received;
    mock->received.identifier = 0U;
    return MCAL_OK;
}

static Mcal_ResultType recover(void *context)
{
    MockType *mock = context;
    mock->recoveries++;
    return mock->recoveryResult;
}

int main(void)
{
    const Mcal_TwaiFrameType standard = {
        .identifier = 0x123U, .data = {1U, 2U}, .length = 2U
    };
    Mcal_TwaiFrameType invalid = standard;
    invalid.identifier = 0x800U;
    assert(Mcal_Twai_ValidateFrame(&standard) == MCAL_OK);
    assert(Mcal_Twai_ValidateFrame(&invalid) == MCAL_INVALID_ARG);
    invalid.flags = MCAL_TWAI_FLAG_EXTENDED;
    invalid.identifier = 0x1FFFFFFFU;
    assert(Mcal_Twai_ValidateFrame(&invalid) == MCAL_OK);
    invalid.length = 9U;
    assert(Mcal_Twai_ValidateFrame(&invalid) == MCAL_INVALID_ARG);

    MockType mock = {.recoveryResult = MCAL_OK};
    Mcal_TwaiHandleType handle;
    assert(Mcal_Twai_Init(&handle, (Mcal_TwaiInterfaceType){
        .context = &mock, .transmit = transmit,
        .receive = receive, .recover = recover
    }) == MCAL_OK);
    assert(Mcal_Twai_Start(&handle) == MCAL_OK);
    assert(Mcal_Twai_Transmit(&handle, &standard) == MCAL_OK);
    assert(Mcal_Twai_Transmit(&handle, &standard) == MCAL_OK);
    assert(Mcal_Twai_PendingTx(&handle) == 2U);
    assert(Mcal_Twai_Service(&handle, 1U) == MCAL_OK);
    assert(mock.transmitted == 1U && Mcal_Twai_PendingTx(&handle) == 1U);
    Mcal_TwaiFrameType received;
    assert(Mcal_Twai_Receive(&handle, &received) == MCAL_OK);
    assert(received.identifier == standard.identifier);
    assert(Mcal_Twai_Receive(&handle, &received) == MCAL_TIMEOUT);
    assert(Mcal_Twai_Service(&handle, 1U) == MCAL_OK);

    Mcal_TwaiFrameType burst[MCAL_TWAI_QUEUE_CAPACITY];
    for (unsigned int i = 0U; i < MCAL_TWAI_QUEUE_CAPACITY; ++i) {
        burst[i] = standard;
        burst[i].identifier = 0x200U + i;
        assert(Mcal_Twai_Transmit(&handle, &burst[i]) == MCAL_OK);
    }
    assert(Mcal_Twai_Transmit(&handle, &standard) == MCAL_BUSY);
    assert(Mcal_Twai_ReportState(&handle, MCAL_TWAI_BUS_OFF) == MCAL_OK);
    assert(Mcal_Twai_Transmit(&handle, &standard) == MCAL_HW_FAIL);
    assert(Mcal_Twai_Recover(&handle) == MCAL_OK);
    assert(mock.recoveries == 1U);
    assert(Mcal_Twai_PendingTx(&handle) == MCAL_TWAI_QUEUE_CAPACITY);
    return 0;
}
