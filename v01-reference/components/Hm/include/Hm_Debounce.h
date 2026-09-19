#ifndef HM_DEBOUNCE_H
#define HM_DEBOUNCE_H

#include <stdint.h>

typedef struct {
    unsigned int failed;
    unsigned int passed;
    unsigned int failedCyclesToSet;
    unsigned int passedCyclesToHeal;
    int active;
} Hm_DebounceType;

void Hm_DebounceInit(Hm_DebounceType *state, unsigned int failedCyclesToSet,
                     unsigned int passedCyclesToHeal);
void Hm_DebounceStep(Hm_DebounceType *state, int failed);

typedef struct {
    Hm_DebounceType sequence;
    uint32_t lastSequence;
    uint32_t rtfCounts[8];
    uint8_t hasSequence;
} Hm_RuntimeType;

void Hm_RuntimeInit(Hm_RuntimeType *state, unsigned int failedCyclesToSet,
                    unsigned int passedCyclesToHeal);
int Hm_RuntimeObserveSequence(Hm_RuntimeType *state, uint32_t sequence);
void Hm_RuntimeRecordRtf(Hm_RuntimeType *state, uint32_t rtf);
uint32_t Hm_RuntimeRtfCount(const Hm_RuntimeType *state, uint32_t rtf);

#endif
