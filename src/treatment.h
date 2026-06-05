#ifndef TREATMENT_H
#define TREATMENT_H

#include <stdint.h>

enum TreatmentState : uint8_t {
    TS_IDLE,
    TS_FIRST_FLUSH,
    TS_COLLECTING,
    TS_DOSING,
    TS_FILTERING,
    TS_TURB_CHECK,
    TS_DISPENSING,
    TS_RETURNING,
    TS_COOLDOWN
};

void treatment_init();
void treatment_tick();   

TreatmentState treatment_get_state();
const char    *treatment_state_name();

float   treatment_last_turbidity2();  
uint8_t treatment_filter_cycles();

void treatment_pause();
void treatment_resume();
void treatment_reset();

#endif 
