#include <assert.h>
#include "Rte_Sample.h"

int main(void)
{
    Rte_SampleType slot = {0, RTE_SAMPLE_INITIAL, 0, 0};
    Rte_SampleType snapshot;
    Rte_SamplePublish(&slot, 42, RTE_SAMPLE_VALID, 1000000);
    Rte_SampleRepublish(&slot);
    Rte_SampleRead(&slot, &snapshot);
    assert(snapshot.value == 42 && snapshot.sampleTimeUs == 1000000 && snapshot.sequence == 1);
    assert(Rte_SampleIsFresh(&snapshot, 1001000, 1));
    assert(!Rte_SampleIsFresh(&snapshot, 1001001, 1));
    return 0;
}
