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
    TS_COOLDOWN,
    TS_FAULT
};

void treatment_init();
void treatment_tick();   // Non-blocking, call every loop iteration

TreatmentState treatment_get_state();
const char    *treatment_state_name();

// Readings captured during treatment cycle (for telemetry)
float   treatment_last_turbidity2();  // Turb2 at TURB_CHECK
uint8_t treatment_filter_cycles();

// MQTT-driven controls
void treatment_pause();
void treatment_resume();
void treatment_reset();

#endif // TREATMENT_H
