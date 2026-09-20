#include "Cal.h"

#include <string.h>

static Cal_ParameterType *find_parameter(Cal_ContextType *context,
                                         uint16_t address)
{
    for (uint8_t i = 0U; i < context->count; ++i) {
        if (context->parameters[i].address == address) {
            return &context->parameters[i];
        }
    }
    return 0;
}

static Cal_ResultType response(uint8_t *data, uint8_t capacity,
                               uint8_t *length, uint8_t result,
                               uint8_t error)
{
    const uint8_t responseLength = error == 0U ? 1U : 2U;
    if (data == 0 || length == 0 || capacity < responseLength) {
        return CAL_FRAME_ERROR;
    }
    data[0] = result;
    if (error != 0U) {
        data[1] = error;
    }
    *length = responseLength;
    return error == 0U ? CAL_OK :
           error == CAL_XCP_ERR_NOT_CONNECTED ? CAL_NOT_CONNECTED :
           error == CAL_XCP_ERR_OUT_OF_RANGE ? CAL_RANGE_ERROR :
           error == CAL_XCP_ERR_CMD_UNKNOWN ? CAL_UNKNOWN_COMMAND : CAL_INVALID;
}

static Cal_ResultType require_connected(Cal_ContextType *context,
                                        uint8_t *responseData,
                                        uint8_t responseCapacity,
                                        uint8_t *responseLength)
{
    return context->connected ? CAL_OK :
        response(responseData, responseCapacity, responseLength,
                 CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_NOT_CONNECTED);
}

Cal_ResultType Cal_Init(Cal_ContextType *context,
                        Cal_ParameterType *parameters, uint8_t count)
{
    if (context == 0 || parameters == 0 || count == 0U ||
        count > CAL_MAX_PARAMETERS) {
        return CAL_INVALID;
    }
    for (uint8_t i = 0U; i < count; ++i) {
        if (parameters[i].value == 0 || parameters[i].minimum > parameters[i].maximum) {
            return CAL_INVALID;
        }
        for (uint8_t j = 0U; j < i; ++j) {
            if (parameters[i].address == parameters[j].address) {
                return CAL_INVALID;
            }
        }
    }
    *context = (Cal_ContextType){
        .parameters = parameters, .count = count
    };
    return CAL_OK;
}

Cal_ResultType Cal_XcpProcess(Cal_ContextType *context,
                              const uint8_t *request, uint8_t requestLength,
                              uint8_t *responseData, uint8_t responseCapacity,
                              uint8_t *responseLength)
{
    if (context == 0 || request == 0 || requestLength == 0U ||
        requestLength > CAL_XCP_CTO_BYTES || responseData == 0 ||
        responseLength == 0) {
        return CAL_INVALID;
    }
    switch (request[0]) {
    case CAL_XCP_CONNECT:
        if (requestLength != 1U) {
            return response(responseData, responseCapacity, responseLength,
                             CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_INVALID);
        }
        context->connected = 1U;
        return response(responseData, responseCapacity, responseLength,
                        CAL_XCP_RESPONSE_OK, 0U);
    case CAL_XCP_GET_STATUS:
        if (requestLength != 1U) {
            return response(responseData, responseCapacity, responseLength,
                             CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_INVALID);
        }
        if (require_connected(context, responseData, responseCapacity,
                              responseLength) != CAL_OK) {
            return CAL_NOT_CONNECTED;
        }
        if (responseCapacity < 4U) {
            return CAL_FRAME_ERROR;
        }
        responseData[0] = CAL_XCP_RESPONSE_OK;
        responseData[1] = context->connected;
        responseData[2] = context->dirty;
        responseData[3] = context->count;
        *responseLength = 4U;
        return CAL_OK;
    case CAL_XCP_DOWNLOAD: {
        if (requestLength != 7U) {
            return response(responseData, responseCapacity, responseLength,
                             CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_INVALID);
        }
        if (require_connected(context, responseData, responseCapacity,
                              responseLength) != CAL_OK) {
            return CAL_NOT_CONNECTED;
        }
        const uint16_t address = (uint16_t)request[1] |
                                 (uint16_t)((uint16_t)request[2] << 8U);
        Cal_ParameterType *parameter = find_parameter(context, address);
        float value;
        (void)memcpy(&value, &request[3], sizeof(value));
        if (parameter == 0 || value < parameter->minimum ||
            value > parameter->maximum) {
            return response(responseData, responseCapacity, responseLength,
                             CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_OUT_OF_RANGE);
        }
        *parameter->value = value;
        context->dirty = 1U;
        return response(responseData, responseCapacity, responseLength,
                        CAL_XCP_RESPONSE_OK, 0U);
    }
    case CAL_XCP_UPLOAD: {
        if (requestLength != 3U) {
            return response(responseData, responseCapacity, responseLength,
                             CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_INVALID);
        }
        if (require_connected(context, responseData, responseCapacity,
                              responseLength) != CAL_OK) {
            return CAL_NOT_CONNECTED;
        }
        Cal_ParameterType *parameter = find_parameter(
            context, (uint16_t)request[1] | (uint16_t)((uint16_t)request[2] << 8U));
        if (parameter == 0 || responseCapacity < 5U) {
            return response(responseData, responseCapacity, responseLength,
                             CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_OUT_OF_RANGE);
        }
        responseData[0] = CAL_XCP_RESPONSE_OK;
        (void)memcpy(&responseData[1], parameter->value, sizeof(float));
        *responseLength = 5U;
        return CAL_OK;
    }
    default:
        return response(responseData, responseCapacity, responseLength,
                        CAL_XCP_RESPONSE_ERROR, CAL_XCP_ERR_CMD_UNKNOWN);
    }
}
