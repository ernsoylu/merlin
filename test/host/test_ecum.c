#include <assert.h>

#include "EcuM.h"

static void test_state_machine(void)
{
    EcuM_ContextType context;
    EcuM_ContextInit(&context, 4U);
    EcuM_RecordInitFailure(&context);
    assert(EcuM_Release(&context, 100) && context.state == ECUM_DEGRADED);
    assert(!EcuM_Release(&context, 200));
    for (unsigned int i = 0; i < 5U; ++i) {
        EcuM_RecordDeadlineFault(&context);
    }
    assert(context.state == ECUM_SHUTDOWN && context.resetRequested);

    EcuM_BootLoopType retained = {.resets = ECUM_BOOT_LOOP_MAX_RESETS,
                                  .windowStartUs = 0};
    assert(EcuM_EvaluateBootLoop(&context, &retained,
                                 MCAL_MCU_RESET_SOFTWARE, 1000));
    assert(context.state == ECUM_SAFE_HALT && !context.resetRequested);
    assert(!context.watchdogSubscribed);

    context.state = ECUM_RUN;
    uint32_t retainedBoots = 3U;
    context.bootLoopCounter = &retainedBoots;
    for (unsigned int i = 0; i < 240U; ++i) {
        EcuM_RecordRunActivation(&context);
    }
    assert(retainedBoots == 0U);
}

static void test_boot_loop(void)
{
    EcuM_ContextType context;
    EcuM_BootLoopType retained = {0};

    /* A power-on arms the window; four more software resets inside it are
     * counted, and only the sixth boot halts. */
    EcuM_ContextInit(&context, 0U);
    assert(!EcuM_EvaluateBootLoop(&context, &retained,
                                  MCAL_MCU_RESET_POWERON, 0));
    assert(retained.resets == 1U);
    for (unsigned int i = 2U; i <= ECUM_BOOT_LOOP_MAX_RESETS; ++i) {
        assert(!EcuM_EvaluateBootLoop(&context, &retained,
                                      MCAL_MCU_RESET_SOFTWARE, (int64_t)i));
        assert(retained.resets == i);
    }
    assert(EcuM_EvaluateBootLoop(&context, &retained,
                                 MCAL_MCU_RESET_SOFTWARE, 10));
    assert(context.state == ECUM_SAFE_HALT);

    /* The same count outside the window is not a boot loop. */
    EcuM_ContextInit(&context, 0U);
    assert(!EcuM_EvaluateBootLoop(&context, &retained, MCAL_MCU_RESET_SOFTWARE,
                                  ECUM_BOOT_LOOP_WINDOW_US + 11));
    assert(retained.resets == 1U && context.state == ECUM_STARTUP);

    /* Undefined retained memory must not be able to disarm the check: a start
     * time in the future would otherwise never expire. */
    retained = (EcuM_BootLoopType){.resets = ECUM_BOOT_LOOP_MAX_RESETS,
                                   .windowStartUs = 1000000};
    EcuM_ContextInit(&context, 0U);
    assert(!EcuM_EvaluateBootLoop(&context, &retained,
                                  MCAL_MCU_RESET_SOFTWARE, 5));
    assert(retained.resets == 1U && retained.windowStartUs == 5);

    /* A brownout re-arms even with a valid window. */
    retained = (EcuM_BootLoopType){.resets = ECUM_BOOT_LOOP_MAX_RESETS,
                                   .windowStartUs = 0};
    EcuM_ContextInit(&context, 0U);
    assert(!EcuM_EvaluateBootLoop(&context, &retained,
                                  MCAL_MCU_RESET_BROWNOUT, 20));
    assert(retained.resets == 1U);

    assert(!EcuM_EvaluateBootLoop(&context, 0, MCAL_MCU_RESET_SOFTWARE, 0));
    assert(!EcuM_EvaluateBootLoop(0, &retained, MCAL_MCU_RESET_SOFTWARE, 0));
}

int main(void)
{
    test_state_machine();
    test_boot_loop();
    return 0;
}
