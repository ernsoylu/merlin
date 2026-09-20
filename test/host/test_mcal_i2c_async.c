#include <assert.h>
#include <string.h>

#include "Mcal_I2c.h"

typedef struct {
    unsigned int writes;
    unsigned int reads;
    Mcal_ResultType result;
} MockType;

static Mcal_ResultType write_register(void *context, uint8_t address,
                                      uint8_t reg, const uint8_t *data,
                                      uint16_t length)
{
    MockType *mock = context;
    assert(address == 0x76U && reg == 0x10U && data != 0 && length == 2U);
    mock->writes++;
    return mock->result;
}

static Mcal_ResultType read_register(void *context, uint8_t address,
                                     uint8_t reg, uint8_t *data,
                                     uint16_t length)
{
    MockType *mock = context;
    assert(address == 0x76U && reg == 0x10U && data != 0 && length == 2U);
    mock->reads++;
    data[0] = 0xA5U;
    data[1] = 0x5AU;
    return mock->result;
}

static void completed(void *context, Mcal_ResultType result)
{
    unsigned int *calls = context;
    assert(result == MCAL_OK);
    (*calls)++;
}

static Mcal_I2cAsyncRequestType read_request(uint8_t *data,
                                             void *completionContext)
{
    return (Mcal_I2cAsyncRequestType){
        .operation = MCAL_I2C_ASYNC_READ_REGISTER,
        .address = 0x76U,
        .reg = 0x10U,
        .readData = data,
        .length = 2U,
        .completion = completed,
        .completionContext = completionContext
    };
}

int main(void)
{
    MockType mock = {.result = MCAL_OK};
    Mcal_I2cAsyncQueueType queue;
    Mcal_I2cAsync_Init(&queue, (Mcal_I2cInterfaceType){
        .context = &mock,
        .writeRegister = write_register,
        .readRegister = read_register
    });

    uint8_t writeData[2] = {1U, 2U};
    unsigned int completions = 0U;
    Mcal_I2cAsyncRequestType write = {
        .operation = MCAL_I2C_ASYNC_WRITE_REGISTER,
        .address = 0x76U,
        .reg = 0x10U,
        .writeData = writeData,
        .length = 2U,
        .completion = completed,
        .completionContext = &completions
    };
    assert(Mcal_I2cAsync_Submit(&queue, &write) == MCAL_OK);
    assert(Mcal_I2cAsync_Submit(&queue, &write) == MCAL_BUSY);
    assert(Mcal_I2cAsync_Pending(&queue) == 1U);
    assert(Mcal_I2cAsync_Service(&queue) == MCAL_OK);
    assert(mock.writes == 1U && write.completed && completions == 1U);

    uint8_t data[4][2] = {{0}};
    Mcal_I2cAsyncRequestType requests[4];
    for (unsigned int i = 0U; i < 4U; ++i) {
        requests[i] = read_request(data[i], &completions);
        assert(Mcal_I2cAsync_Submit(&queue, &requests[i]) == MCAL_OK);
    }
    Mcal_I2cAsyncRequestType full = read_request(data[0], &completions);
    assert(Mcal_I2cAsync_Submit(&queue, &full) == MCAL_BUSY);
    assert(Mcal_I2cAsync_Cancel(&queue, &requests[1]) == MCAL_OK);
    assert(Mcal_I2cAsync_Pending(&queue) == 3U);
    while (Mcal_I2cAsync_Pending(&queue) != 0U) {
        assert(Mcal_I2cAsync_Service(&queue) == MCAL_OK);
    }
    assert(mock.reads == 3U);

    uint8_t boundedData[2] = {0U, 0U};
    Mcal_I2cAsyncRequestType bounded = read_request(boundedData, &completions);
    assert(Mcal_I2c_TransferBounded(&queue, &bounded, 1U) == MCAL_OK);
    assert(memcmp(boundedData, (uint8_t[]){0xA5U, 0x5AU}, 2U) == 0);

    Mcal_I2cAsyncRequestType blocker = read_request(data[0], &completions);
    Mcal_I2cAsyncRequestType delayed = read_request(data[1], &completions);
    assert(Mcal_I2cAsync_Submit(&queue, &blocker) == MCAL_OK);
    assert(Mcal_I2c_TransferBounded(&queue, &delayed, 1U) == MCAL_TIMEOUT);
    assert(!delayed.completed && !delayed.queued);
    assert(Mcal_I2cAsync_Pending(&queue) == 0U);
    assert(blocker.completed);
    assert(Mcal_I2c_TransferBounded(&queue, &delayed, 0U) == MCAL_TIMEOUT);

    Mcal_I2cAsyncRequestType invalid = read_request(0, &completions);
    assert(Mcal_I2cAsync_Submit(&queue, &invalid) == MCAL_INVALID_ARG);
    return 0;
}
