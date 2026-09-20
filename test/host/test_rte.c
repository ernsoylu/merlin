#include <assert.h>

#include "Rte_Sample.h"
#include "Rte_Type.h"

static void test_scalar_sample(void)
{
    Rte_SampleType slot = {0, RTE_SAMPLE_INITIAL, 0, 0};
    Rte_SampleType snapshot;

    Rte_SamplePublish(&slot, 42, RTE_SAMPLE_VALID, 1000000);
    Rte_SampleRepublish(&slot);
    Rte_SampleRead(&slot, &snapshot);
    assert(snapshot.value == 42 && snapshot.sampleTimeUs == 1000000 && snapshot.sequence == 1);
    assert(Rte_SampleIsFresh(&snapshot, 1001000, 1));
    assert(!Rte_SampleIsFresh(&snapshot, 1001001, 1));
}

static Rte_EnvironmentalDataType valid_sample(int64_t sampleTimeUs)
{
    Rte_EnvironmentalDataType sample = {
        .temperatureDegC = 21.5f,
        .humidityPercent = 40.0f,
        .pressurePa = 101325.0f,
        .quality = {RTE_QUALITY_VALID, RTE_QUALITY_VALID, RTE_QUALITY_VALID},
        .sampleTimeUs = sampleTimeUs,
        .sequence = 7U
    };
    return sample;
}

static void test_environmental_slot(void)
{
    Rte_EnvironmentalSlotType slot = {0};
    Rte_EnvironmentalDataType sample = valid_sample(1000000);
    Rte_EnvironmentalDataType out;

    /* The slot carries the acquisition's own identity, not a publication
     * count: a consumer comparing it against the provider's sequence must
     * see the same number. */
    Rte_EnvironmentalPublish(&slot, &sample);
    Rte_EnvironmentalRead(&slot, &out);
    assert(out.sequence == 7U);
    assert(out.sampleTimeUs == 1000000);
    assert(out.temperatureDegC == 21.5f && out.pressurePa == 101325.0f);

    /* Republishing the same cached acquisition ages nothing and counts
     * nothing -- that is what keeps staleness detection working downstream. */
    Rte_EnvironmentalPublish(&slot, &sample);
    Rte_EnvironmentalRead(&slot, &out);
    assert(out.sequence == 7U);
    assert(out.sampleTimeUs == 1000000);
}

static void test_environmental_freshness(void)
{
    Rte_EnvironmentalDataType sample = valid_sample(1000000);

    /* maxAgeMs belongs to the consumer and is evaluated at read time. */
    assert(Rte_EnvironmentalIsFresh(&sample, 1000000, 50U));
    assert(Rte_EnvironmentalIsFresh(&sample, 1050000, 50U));
    assert(!Rte_EnvironmentalIsFresh(&sample, 1050001, 50U));

    /* A reading from the future is not fresh, it is wrong. */
    assert(!Rte_EnvironmentalIsFresh(&sample, 999999, 50U));

    /* Every channel has to be valid: one bad channel spoils the sample. */
    for (unsigned int i = 0; i < 3U; ++i) {
        Rte_EnvironmentalDataType partial = valid_sample(1000000);
        partial.quality[i] = RTE_QUALITY_INVALID;
        assert(!Rte_EnvironmentalIsFresh(&partial, 1000000, 50U));
        partial.quality[i] = RTE_QUALITY_INITIAL;
        assert(!Rte_EnvironmentalIsFresh(&partial, 1000000, 50U));
    }

    assert(!Rte_EnvironmentalIsFresh(0, 1000000, 50U));
}

static void test_environmental_xcore_slot(void)
{
    Rte_EnvironmentalXcoreSlotType slot = {0};
    Rte_EnvironmentalDataType sample = valid_sample(2000000);
    Rte_EnvironmentalDataType out = {0};

    assert(!Rte_EnvironmentalXcoreRead(0, &out));
    assert(!Rte_EnvironmentalXcoreRead(&slot, 0));
    __atomic_store_n(&slot.version, 1U, __ATOMIC_RELEASE);
    assert(!Rte_EnvironmentalXcoreRead(&slot, &out));
    Rte_EnvironmentalXcorePublish(&slot, &sample);
    assert(Rte_EnvironmentalXcoreRead(&slot, &out));
    assert(out.sequence == sample.sequence &&
           out.sampleTimeUs == sample.sampleTimeUs &&
           out.temperatureDegC == sample.temperatureDegC);
    assert((__atomic_load_n(&slot.version, __ATOMIC_ACQUIRE) & 1U) == 0U);
}

static void event_handler(uint32_t event, void *context)
{
    uint32_t *last = context;
    *last = event;
}

static void test_event_queue(void)
{
    Rte_EventQueueType queue;
    const Rte_EventQueueConfigType invalid = {
        .burst = 0U, .serviceRate = 1U,
        .fullPolicy = RTE_EVENT_DROP_NEWEST
    };
    const Rte_EventQueueConfigType config = {
        .burst = 2U, .serviceRate = 1U,
        .fullPolicy = RTE_EVENT_DROP_NEWEST
    };
    assert(Rte_EventQueueInit(&queue, &invalid) == RTE_EVENT_INVALID);
    assert(Rte_EventQueueInit(&queue, &config) == RTE_EVENT_OK);
    assert(Rte_EventQueueInit(0, &config) == RTE_EVENT_INVALID);
    assert(Rte_EventQueuePush(&queue, 10U) == RTE_EVENT_OK);
    assert(Rte_EventQueuePush(&queue, 11U) == RTE_EVENT_OK);
    assert(Rte_EventQueuePending(&queue) == 2U);
    uint32_t last = 0U;
    assert(Rte_EventQueueService(&queue, event_handler, &last) == 1U);
    assert(last == 10U && Rte_EventQueuePending(&queue) == 1U);

    uint32_t event = 0U;
    assert(Rte_EventQueuePush(&queue, 12U) == RTE_EVENT_OK);
    assert(Rte_EventQueuePush(&queue, 13U) == RTE_EVENT_OK);
    assert(Rte_EventQueuePush(&queue, 14U) == RTE_EVENT_OK);
    assert(Rte_EventQueuePush(&queue, 15U) == RTE_EVENT_FULL);
    assert(queue.dropped == 1U);
    assert(Rte_EventQueuePop(&queue, &event) == RTE_EVENT_OK && event == 11U);
    assert(Rte_EventQueuePop(&queue, &event) == RTE_EVENT_OK && event == 12U);
    assert(Rte_EventQueuePop(&queue, &event) == RTE_EVENT_OK && event == 13U);
    assert(Rte_EventQueuePop(&queue, &event) == RTE_EVENT_OK && event == 14U);
    assert(Rte_EventQueuePop(&queue, &event) == RTE_EVENT_FULL);

    const Rte_EventQueueConfigType dropOldest = {
        .burst = 4U, .serviceRate = 4U,
        .fullPolicy = RTE_EVENT_DROP_OLDEST
    };
    assert(Rte_EventQueueInit(&queue, &dropOldest) == RTE_EVENT_OK);
    for (uint32_t value = 1U; value <= RTE_EVENT_QUEUE_CAPACITY; ++value) {
        assert(Rte_EventQueuePush(&queue, value) == RTE_EVENT_OK);
    }
    assert(Rte_EventQueuePush(&queue, 5U) == RTE_EVENT_OK);
    assert(queue.dropped == 1U);
    assert(Rte_EventQueuePop(&queue, &event) == RTE_EVENT_OK && event == 2U);
}

int main(void)
{
    test_scalar_sample();
    test_environmental_slot();
    test_environmental_freshness();
    test_environmental_xcore_slot();
    test_event_queue();
    return 0;
}
