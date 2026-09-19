#ifndef MCAL_I2C_H
#define MCAL_I2C_H

#include <stdint.h>

#include "Std_Types.h"

typedef Mcal_ResultType (*Mcal_I2cWriteRegisterFn)(void *context,
                                                   uint8_t address,
                                                   uint8_t reg,
                                                   const uint8_t *data,
                                                   uint16_t length);
typedef Mcal_ResultType (*Mcal_I2cReadRegisterFn)(void *context,
                                                  uint8_t address,
                                                  uint8_t reg,
                                                  uint8_t *data,
                                                  uint16_t length);
typedef Mcal_ResultType (*Mcal_I2cRecoverFn)(void *context);

typedef struct {
    void *context;
    Mcal_I2cWriteRegisterFn writeRegister;
    Mcal_I2cReadRegisterFn readRegister;
    Mcal_I2cRecoverFn recover;
} Mcal_I2cInterfaceType;

typedef struct {
    int32_t port;
    int32_t sdaPin;
    int32_t sclPin;
    uint32_t frequencyHz;
    uint32_t timeoutMs;
    uint8_t pullups;
} Mcal_I2cConfigType;

typedef struct {
    int32_t port;
    uint32_t timeoutMs;
    int32_t sdaPin;
    int32_t sclPin;
    uint32_t frequencyHz;
    uint8_t pullups;
    uint8_t initialized;
} Mcal_I2cHandleType;

Mcal_ResultType Mcal_I2c_Init(Mcal_I2cHandleType *handle,
                              const Mcal_I2cConfigType *config);
Mcal_I2cInterfaceType Mcal_I2c_GetInterface(Mcal_I2cHandleType *handle);
Mcal_ResultType Mcal_I2c_Recover(Mcal_I2cHandleType *handle);

#endif
