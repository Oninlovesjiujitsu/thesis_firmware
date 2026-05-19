#include <Arduino.h>
#include "config.h"
#include "treatment.h"
#include "actuator.h"
#include "sensors.h"

// ---------------------------------------------------------------------------
// State names in PROGMEM
// ---------------------------------------------------------------------------
static const char S_IDLE[]       PROGMEM = "IDLE";
static const char S_FILTERING[]  PROGMEM = "FILTERING";
static const char S_SETTLING[]   PROGMEM = "SETTLING";
static const char S_QC[]         PROGMEM = "QUALITY_CHECK";
static const char S_DOSING[]     PROGMEM = "DOSING";
static const char S_DISPENSING[] PROGMEM = "DISPENSING";
static const char S_RETURNING[]  PROGMEM = "RETURNING";
static const char S_COOLDOWN[]   PROGMEM = "COOLDOWN";
static const char S_FAULT[]      PROGMEM = "FAULT";

static const char * const STATE_NAMES[] PROGMEM = {
    S_IDLE, S_FILTERING, S_SETTLING, S_QC, S_DOSING,
    S_DISPENSING, S_RETURNING, S_COOLDOWN, S_FAULT
};

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static TreatmentState state      = TS_IDLE;
static unsigned long  stateEntry = 0;
static bool           paused     = false;

// Float switch debounce
static unsigned long debounceStart  = 0;
static bool          debounceActive = false;

// Quality check results (retained for telemetry between cycles)
static float   lastTurb2      = 0.0f;
static float   lastPh         = 0.0f;
static uint8_t doseAttempts   = 0;
static uint8_t filterCycles   = 0;

// Dosing-cycle flag: true while dose→filter→return→settle→recheck loop is active
static bool dosingCycle = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void enter_state(TreatmentState next) {
    Serial.printf("[TREATMENT] %s -> %s\n", STATE_NAMES[state], STATE_NAMES[next]);
    state      = next;
    stateEntry = millis();
}

static void enter_fault(const char *reason) {
    Serial.printf("[TREATMENT] FAULT: %s\n", reason);
    actuator_all_off();
    state      = TS_FAULT;
    stateEntry = millis();
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------

static void tick_idle(unsigned long now) {
    bool water = sensors_read_float_switch();
    if (water) {
        if (!debounceActive) {
            debounceActive = true;
            debounceStart  = now;
        } else if (now - debounceStart >= FLOAT_DEBOUNCE_MS) {
            debounceActive = false;

            lastPh = sensors_read_ph();
            Serial.printf("[TREATMENT] Float triggered — pH=%.2f\n", lastPh);

            if (lastPh < PH_MIN || lastPh > PH_MAX) {
                dosingCycle  = true;
                doseAttempts = 1;
                if (lastPh > PH_MAX) {
                    dose_acid_set(true);
                    Serial.println("[TREATMENT] pH high — dosing acid");
                } else {
                    dose_base_set(true);
                    Serial.println("[TREATMENT] pH low — dosing base");
                }
                enter_state(TS_DOSING);
            } else {
                Serial.println("[TREATMENT] pH OK — filtering");
                sol2_set(true);
                pump1_set(true);
                enter_state(TS_FILTERING);
            }
        }
    } else {
        debounceActive = false;
    }
}

static void tick_filtering(unsigned long now) {
    unsigned long elapsed = now - stateEntry;
    bool water = sensors_read_float_switch();

    if (elapsed >= FILTER_MAX_RUN_MS || !water) {
        pump1_set(false);
        sol2_set(false);
        Serial.printf("[TREATMENT] Filtering done after %lus\n", elapsed / 1000UL);

        if (dosingCycle) {
            // Return water to Tank 1 for pH re-check
            sol4_set(true);
            pump2_set(true);
            enter_state(TS_RETURNING);
        } else {
            enter_state(TS_SETTLING);
        }
    }
}

static void tick_settling(unsigned long now) {
    if (now - stateEntry >= SETTLE_MS) {
        if (dosingCycle) {
            // Re-check pH after filter→return mix cycle
            lastPh = sensors_read_ph();
            Serial.printf("[TREATMENT] pH re-check #%d: %.2f\n", doseAttempts, lastPh);

            if (lastPh >= PH_MIN && lastPh <= PH_MAX) {
                dosingCycle = false;
                Serial.println("[TREATMENT] pH OK — final filtering");
                sol2_set(true);
                pump1_set(true);
                enter_state(TS_FILTERING);
            } else if (doseAttempts >= MAX_DOSE_ATTEMPTS) {
                dosingCycle = false;
                Serial.println("[TREATMENT] Max dose attempts — filtering anyway");
                sol2_set(true);
                pump1_set(true);
                enter_state(TS_FILTERING);
            } else {
                doseAttempts++;
                if (lastPh > PH_MAX) {
                    dose_acid_set(true);
                    Serial.println("[TREATMENT] pH still high — re-dosing acid");
                } else {
                    dose_base_set(true);
                    Serial.println("[TREATMENT] pH still low — re-dosing base");
                }
                enter_state(TS_DOSING);
            }
        } else {
            enter_state(TS_QUALITY_CHECK);
        }
    }
}

static void tick_quality_check() {
    lastTurb2 = sensors_read_turbidity2();
    Serial.printf("[TREATMENT] QC: turb2=%.1f NTU\n", lastTurb2);

    if (lastTurb2 > TURBIDITY_CLEAN_NTU) {
        filterCycles++;
        if (filterCycles > MAX_FILTER_CYCLES) {
            enter_fault("Max filter cycles exceeded");
            return;
        }
        Serial.printf("[TREATMENT] Turbid — returning (cycle %u/%u)\n",
                      filterCycles, (uint8_t)MAX_FILTER_CYCLES);
        sol4_set(true);
        pump2_set(true);
        enter_state(TS_RETURNING);
    } else {
        Serial.println("[TREATMENT] Water clean — dispensing");
        filterCycles = 0;
        sol3_set(true);
        pump2_set(true);
        enter_state(TS_DISPENSING);
    }
}

static void tick_dosing(unsigned long now) {
    if (now - stateEntry >= DOSE_PULSE_MS) {
        dose_acid_set(false);
        dose_base_set(false);
        Serial.println("[TREATMENT] Dose done — filtering to mix");
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FILTERING);
    }
}

static void tick_dispensing(unsigned long now) {
    if (now - stateEntry >= DISPENSE_RUN_MS) {
        pump2_set(false);
        sol3_set(false);
        enter_state(TS_COOLDOWN);
    }
}

static void tick_returning(unsigned long now) {
    if (now - stateEntry >= RETURN_RUN_MS) {
        pump2_set(false);
        sol4_set(false);

        if (dosingCycle) {
            enter_state(TS_SETTLING);   // Settle then re-check pH
        } else {
            enter_state(TS_COOLDOWN);
        }
    }
}

static void tick_cooldown(unsigned long now) {
    if (now - stateEntry >= COOLDOWN_MS) {
        actuator_all_off();   // Defensive cleanup
        enter_state(TS_IDLE);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void treatment_init() {
    state        = TS_IDLE;
    stateEntry   = millis();
    paused       = false;
    dosingCycle   = false;
    doseAttempts  = 0;
    filterCycles  = 0;
    Serial.println("[TREATMENT] Init — IDLE");
}

void treatment_tick() {
    if (paused || state == TS_FAULT) return;

    unsigned long now = millis();

    switch (state) {
    case TS_IDLE:          tick_idle(now);          break;
    case TS_FILTERING:     tick_filtering(now);     break;
    case TS_SETTLING:      tick_settling(now);      break;
    case TS_QUALITY_CHECK: tick_quality_check();    break;
    case TS_DOSING:        tick_dosing(now);        break;
    case TS_DISPENSING:    tick_dispensing(now);     break;
    case TS_RETURNING:     tick_returning(now);     break;
    case TS_COOLDOWN:      tick_cooldown(now);      break;
    case TS_FAULT:         break;  // Handled above, but silences warning
    }
}

TreatmentState treatment_get_state() {
    return state;
}

const char *treatment_state_name() {
    return STATE_NAMES[state];
}

float treatment_last_turbidity2() {
    return lastTurb2;
}

float treatment_last_ph() {
    return lastPh;
}

uint8_t treatment_dose_attempts() {
    return doseAttempts;
}

uint8_t treatment_filter_cycles() {
    return filterCycles;
}

void treatment_pause() {
    if (state == TS_FAULT) return;
    paused = true;
    actuator_all_off();
    Serial.println("[TREATMENT] PAUSED — all actuators OFF");
}

void treatment_resume() {
    if (state == TS_FAULT) {
        Serial.println("[TREATMENT] Cannot resume from FAULT — use RESET");
        return;
    }
    paused = false;
    // Re-entering mid-state is unsafe — reset to IDLE
    actuator_all_off();
    dosingCycle = false;
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESUMED — restarting from IDLE");
}

void treatment_reset() {
    paused = false;
    actuator_all_off();
    dosingCycle   = false;
    doseAttempts  = 0;
    filterCycles  = 0;
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESET — IDLE");
}
