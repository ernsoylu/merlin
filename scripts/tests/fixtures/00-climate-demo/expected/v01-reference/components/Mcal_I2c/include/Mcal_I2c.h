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

#define MCAL_I2C_ASYNC_CAPACITY 4U

typedef enum {
    MCAL_I2C_ASYNC_WRITE_REGISTER = 0,
    MCAL_I2C_ASYNC_READ_REGISTER
} Mcal_I2cAsyncOperationType;

typedef void (*Mcal_I2cAsyncCompletionFn)(void *context,
                                          Mcal_ResultType result);

typedef struct {
    Mcal_I2cAsyncOperationType operation;
    uint8_t address;
    uint8_t reg;
    const uint8_t *writeData;
    uint8_t *readData;
    uint16_t length;
    Mcal_I2cAsyncCompletionFn completion;
    void *completionContext;
    Mcal_ResultType result;
    uint8_t completed;
    uint8_t queued;
} Mcal_I2cAsyncRequestType;

typedef struct {
    Mcal_I2cInterfaceType interface;
    Mcal_I2cAsyncRequestType *requests[MCAL_I2C_ASYNC_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} Mcal_I2cAsyncQueueType;

Mcal_ResultType Mcal_I2c_Init(Mcal_I2cHandleType *handle,
                              const Mcal_I2cConfigType *config);
Mcal_I2cInterfaceType Mcal_I2c_GetInterface(Mcal_I2cHandleType *handle);
Mcal_ResultType Mcal_I2c_Recover(Mcal_I2cHandleType *handle);

void Mcal_I2cAsync_Init(Mcal_I2cAsyncQueueType *queue,
                        Mcal_I2cInterfaceType interface);
Mcal_ResultType Mcal_I2cAsync_Submit(Mcal_I2cAsyncQueueType *queue,
                                     Mcal_I2cAsyncRequestType *request);
Mcal_ResultType Mcal_I2cAsync_Service(Mcal_I2cAsyncQueueType *queue);
Mcal_ResultType Mcal_I2cAsync_Cancel(Mcal_I2cAsyncQueueType *queue,
                                     Mcal_I2cAsyncRequestType *request);
Mcal_ResultType Mcal_I2c_TransferBounded(Mcal_I2cAsyncQueueType *queue,
                                         Mcal_I2cAsyncRequestType *request,
                                         uint8_t serviceBudget);
uint8_t Mcal_I2cAsync_Pending(const Mcal_I2cAsyncQueueType *queue);

#endif
