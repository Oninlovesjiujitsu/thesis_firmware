#ifndef TREATMENT_H
#define TREATMENT_H

#include <stdint.h>

enum TreatmentState : uint8_t {
    TS_IDLE,
    TS_FILTERING,
    TS_SETTLING,
    TS_QUALITY_CHECK,
    TS_DOSING,
    TS_DISPENSING,
    TS_RETURNING,
    TS_COOLDOWN,
    TS_FAULT
};

void treatment_init();
void treatment_tick();   // Non-blocking, call every loop iteration

TreatmentState treatment_get_state();
const char    *treatment_state_name();

// Last readings captured at TS_QUALITY_CHECK (for telemetry)
float   treatment_last_turbidity2();
float   treatment_last_ph();
uint8_t treatment_dose_attempts();

// MQTT-driven controls
void treatment_pause();
void treatment_resume();
void treatment_reset();

#endif // TREATMENT_H
