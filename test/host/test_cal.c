#include <assert.h>
#include <string.h>

#include "Cal.h"

static void frame_float(uint8_t *frame, uint16_t address, float value)
{
    frame[0] = CAL_XCP_DOWNLOAD;
    frame[1] = (uint8_t)address;
    frame[2] = (uint8_t)(address >> 8);
    (void)memcpy(&frame[3], &value, sizeof(value));
}

int main(void)
{
    float kp = 0.1f;
    float ki = 0.02f;
    Cal_ParameterType parameters[] = {
        {.address = 0x10U, .value = &kp, .minimum = 0.0f, .maximum = 10.0f},
        {.address = 0x11U, .value = &ki, .minimum = 0.0f, .maximum = 1.0f}
    };
    Cal_ContextType context;
    uint8_t request[CAL_XCP_CTO_BYTES] = {0};
    uint8_t response[CAL_XCP_CTO_BYTES] = {0};
    uint8_t responseLength = 0U;
    assert(Cal_Init(&context, parameters, 2U) == CAL_OK);
    assert(Cal_XcpProcess(&context, (uint8_t[]){CAL_XCP_GET_STATUS}, 1U,
                          response, sizeof(response), &responseLength) ==
           CAL_NOT_CONNECTED);
    assert(Cal_XcpProcess(&context, (uint8_t[]){CAL_XCP_CONNECT}, 1U,
                          response, sizeof(response), &responseLength) == CAL_OK);

    frame_float(request, 0x10U, 0.25f);
    assert(Cal_XcpProcess(&context, request, 7U, response, sizeof(response),
                          &responseLength) == CAL_OK);
    assert(kp == 0.25f && context.dirty);

    frame_float(request, 0x10U, 11.0f);
    assert(Cal_XcpProcess(&context, request, 7U, response, sizeof(response),
                          &responseLength) == CAL_RANGE_ERROR);
    assert(kp == 0.25f && response[0] == CAL_XCP_RESPONSE_ERROR);

    request[0] = CAL_XCP_UPLOAD;
    request[1] = 0x10U;
    request[2] = 0U;
    assert(Cal_XcpProcess(&context, request, 3U, response, sizeof(response),
                          &responseLength) == CAL_OK);
    float uploaded;
    (void)memcpy(&uploaded, &response[1], sizeof(uploaded));
    assert(uploaded == 0.25f && responseLength == 5U);

    request[0] = 0x01U;
    assert(Cal_XcpProcess(&context, request, 1U, response, sizeof(response),
                          &responseLength) == CAL_UNKNOWN_COMMAND);
    assert(Cal_Init(&context, parameters, 0U) == CAL_INVALID);
    return 0;
}
