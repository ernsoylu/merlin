#ifndef HM_DEBOUNCE_H
#define HM_DEBOUNCE_H

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

#endif
