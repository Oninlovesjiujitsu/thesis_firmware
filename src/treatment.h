#ifndef TREATMENT_H
#define TREATMENT_H

#include <stdint.h>

enum TreatmentState : uint8_t {
    TS_IDLE,
    TS_FIRST_FLUSH,
    TS_COLLECTING,
    TS_SENSING,
    TS_DOSING,
    TS_MIXING,
    TS_RETURNING,
    TS_SETTLE,
    TS_PH_CHECK,
    TS_FINAL_FILTER,
    TS_TURB_CHECK,
    TS_DISPENSING,
    TS_COOLDOWN,
    TS_FAULT
};

void treatment_init();
void treatment_tick();   // Non-blocking, call every loop iteration

TreatmentState treatment_get_state();
const char    *treatment_state_name();

// Readings captured during treatment cycle (for telemetry)
float   treatment_last_turbidity1();  // Turb1 at SENSING
float   treatment_last_turbidity2();  // Turb2 at TURB_CHECK
float   treatment_last_ph();          // pH at SENSING
uint8_t treatment_dose_attempts();
uint8_t treatment_filter_cycles();
bool    treatment_warn_ph_max();      // true if pH correction gave up

// MQTT-driven controls
void treatment_pause();
void treatment_resume();
void treatment_reset();

#endif // TREATMENT_H
