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

// Dosing sub-phase: 0 = dosing pulse, 1 = mixing wait, 2 = re-check
static uint8_t dosingPhase      = 0;
static unsigned long dosingTimer = 0;

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

            Serial.println("[TREATMENT] Float triggered — filtering (pH bypassed)");
            sol2_set(true);
            pump1_set(true);
            enter_state(TS_FILTERING);
        }
    } else {
        debounceActive = false;
    }
}

static void tick_filtering(unsigned long now) {
    unsigned long elapsed = now - stateEntry;
    bool water = sensors_read_float_switch();

    if (elapsed >= FILTER_MAX_RUN_MS) {
        // Safety timeout — tank should be empty by now
        pump1_set(false);
        sol2_set(false);
        Serial.printf("[TREATMENT] Filter safety timeout %lus\n", elapsed / 1000UL);
        enter_state(TS_SETTLING);
    } else if (!water) {
        // Tank 1 drained — normal end of filtering
        pump1_set(false);
        sol2_set(false);
        Serial.printf("[TREATMENT] Tank 1 empty after %lus\n", elapsed / 1000UL);
        enter_state(TS_SETTLING);
    }
}

static void tick_settling(unsigned long now) {
    if (now - stateEntry >= SETTLE_MS) {
        enter_state(TS_QUALITY_CHECK);
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
    switch (dosingPhase) {
    case 0: // Dosing pulse active
        if (now - dosingTimer >= DOSE_PULSE_MS) {
            dose_acid_set(false);
            dose_base_set(false);
            dosingPhase = 1;
            dosingTimer = now;
            Serial.println("[TREATMENT] Dose pulse done — mixing");
        }
        break;

    case 1: // Mixing wait
        if (now - dosingTimer >= DOSE_MIX_MS) {
            dosingPhase = 2;
            Serial.println("[TREATMENT] Mix done — re-checking pH");
        }
        break;

    case 2: { // Re-check pH
        lastPh = sensors_read_ph();
        doseAttempts++;
        Serial.printf("[TREATMENT] pH re-check #%d: %.2f\n", doseAttempts, lastPh);

        if (lastPh >= PH_MIN && lastPh <= PH_MAX) {
            // pH corrected — proceed to filtering
            Serial.println("[TREATMENT] pH corrected — filtering");
            sol2_set(true);
            pump1_set(true);
            enter_state(TS_FILTERING);
        } else if (doseAttempts >= MAX_DOSE_ATTEMPTS) {
            // Max attempts — filter anyway with warning
            Serial.println("[TREATMENT] WARNING: max dose attempts — filtering anyway");
            sol2_set(true);
            pump1_set(true);
            enter_state(TS_FILTERING);
        } else {
            // Try again
            dosingPhase = 0;
            dosingTimer = now;
            if (lastPh > PH_MAX) {
                dose_acid_set(true);
            } else {
                dose_base_set(true);
            }
            Serial.println("[TREATMENT] Re-dosing");
        }
        break;
    }
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
        enter_state(TS_COOLDOWN);
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
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESUMED — restarting from IDLE");
}

void treatment_reset() {
    paused = false;
    actuator_all_off();
    doseAttempts  = 0;
    filterCycles  = 0;
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESET — IDLE");
}
