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

int main(void)
{
    test_scalar_sample();
    test_environmental_slot();
    test_environmental_freshness();
    return 0;
}
