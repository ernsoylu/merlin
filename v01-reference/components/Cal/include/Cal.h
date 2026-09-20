#ifndef CAL_H
#define CAL_H

#include <stdint.h>

#define CAL_MAX_PARAMETERS 16U
#define CAL_XCP_CTO_BYTES 8U

#define CAL_XCP_CONNECT 0xFFU
#define CAL_XCP_GET_STATUS 0xFDU
#define CAL_XCP_UPLOAD 0xF5U
#define CAL_XCP_DOWNLOAD 0xF0U
#define CAL_XCP_RESPONSE_OK 0xFFU
#define CAL_XCP_RESPONSE_ERROR 0xFEU

typedef enum {
    CAL_OK = 0,
    CAL_INVALID,
    CAL_NOT_CONNECTED,
    CAL_UNKNOWN_COMMAND,
    CAL_RANGE_ERROR,
    CAL_FRAME_ERROR
} Cal_ResultType;

typedef enum {
    CAL_XCP_ERR_INVALID = 0x21U,
    CAL_XCP_ERR_NOT_CONNECTED = 0x22U,
    CAL_XCP_ERR_OUT_OF_RANGE = 0x23U,
    CAL_XCP_ERR_CMD_UNKNOWN = 0x20U
} Cal_XcpErrorType;

typedef struct {
    uint16_t address;
    float *value;
    float minimum;
    float maximum;
} Cal_ParameterType;

typedef struct {
    Cal_ParameterType *parameters;
    uint8_t count;
    uint8_t connected;
    uint8_t dirty;
} Cal_ContextType;

Cal_ResultType Cal_Init(Cal_ContextType *context,
                        Cal_ParameterType *parameters, uint8_t count);
Cal_ResultType Cal_XcpProcess(Cal_ContextType *context,
                              const uint8_t *request, uint8_t requestLength,
                              uint8_t *response, uint8_t responseCapacity,
                              uint8_t *responseLength);

#endif
