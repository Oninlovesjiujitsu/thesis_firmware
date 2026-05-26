#include <Arduino.h>
#include "config.h"
#include "treatment.h"
#include "actuator.h"
#include "sensors.h"

// ---------------------------------------------------------------------------
// State names in PROGMEM
// ---------------------------------------------------------------------------
static const char S_IDLE[]       PROGMEM = "IDLE";
static const char S_FIRST_FLUSH[] PROGMEM = "FIRST_FLUSH";
static const char S_COLLECTING[] PROGMEM = "COLLECTING";
static const char S_DOSING[]     PROGMEM = "DOSING";
static const char S_FILTERING[]  PROGMEM = "FILTERING";
static const char S_TURB_CHECK[] PROGMEM = "TURB_CHECK";
static const char S_DISPENSING[] PROGMEM = "DISPENSING";
static const char S_RETURNING[]  PROGMEM = "RETURNING";
static const char S_COOLDOWN[]   PROGMEM = "COOLDOWN";

static const char * const STATE_NAMES[] PROGMEM = {
    S_IDLE, S_FIRST_FLUSH, S_COLLECTING, S_DOSING, S_FILTERING,
    S_TURB_CHECK, S_DISPENSING, S_RETURNING, S_COOLDOWN
};

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static TreatmentState state      = TS_IDLE;
static unsigned long  stateEntry = 0;
static bool           paused     = false;

// Debounce (shared between rain and float switch)
static unsigned long debounceStart  = 0;
static bool          debounceActive = false;

// Rain read throttle
static unsigned long lastRainCheck = 0;

// Cycle data (retained for telemetry between cycles)
static float   lastTurb2      = 0.0f;
static uint8_t filterCycles   = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void enter_state(TreatmentState next) {
    Serial.printf("[TREATMENT] %s -> %s\n", STATE_NAMES[state], STATE_NAMES[next]);
    state      = next;
    stateEntry = millis();
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------

static void tick_idle(unsigned long now) {
    // Throttle rain sensor reads to 1/s
    if (now - lastRainCheck < RAIN_CHECK_INTERVAL_MS) return;
    lastRainCheck = now;

    float rain = sensors_read_rain();
    bool raining = (rain < RAIN_TRIGGER_THRESHOLD);

    if (raining) {
        if (!debounceActive) {
            debounceActive = true;
            debounceStart  = now;
        } else if (now - debounceStart >= RAIN_DEBOUNCE_MS) {
            debounceActive = false;
            Serial.printf("[TREATMENT] Rain detected (%.3f) — first flush\n", rain);
            sol1_set(true);
            enter_state(TS_FIRST_FLUSH);
        }
    } else {
        debounceActive = false;
    }
}

static void tick_first_flush(unsigned long now) {
    if (now - stateEntry >= FIRST_FLUSH_MS) {
        sol1_set(false);
        Serial.println("[TREATMENT] First flush done — collecting");
        enter_state(TS_COLLECTING);
    }
}

static void tick_collecting(unsigned long now) {
    bool full = sensors_read_float_switch();

    if (full) {
        if (!debounceActive) {
            debounceActive = true;
            debounceStart  = now;
        } else if (now - debounceStart >= FLOAT_DEBOUNCE_MS) {
            debounceActive = false;
            filterCycles = 0;
            Serial.println("[TREATMENT] Tank1 full — dosing base");
            sol1_set(true);        // Divert rain away from tank1 during treatment
            dose_base_set(true);
            enter_state(TS_DOSING);
        }
    } else {
        debounceActive = false;
    }
}

static void tick_dosing(unsigned long now) {
    if (now - stateEntry >= DOSE_BASE_FIXED_MS) {
        dose_base_set(false);
        Serial.println("[TREATMENT] Dose done — filtering");
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FILTERING);
    }
}

static void tick_filtering(unsigned long now) {
    // Run until tank1 is empty (float switch LOW), debounced
    bool empty = !sensors_read_float_switch();

    if (empty) {
        if (!debounceActive) {
            debounceActive = true;
            debounceStart  = now;
        } else if (now - debounceStart >= FLOAT_DEBOUNCE_MS) {
            debounceActive = false;
            pump1_set(false);
            sol2_set(false);
            Serial.println("[TREATMENT] Tank1 empty — turbidity check");
            enter_state(TS_TURB_CHECK);
        }
    } else {
        debounceActive = false;
    }
}

static void tick_turb_check() {
    lastTurb2 = sensors_read_turbidity2();
    Serial.printf("[TREATMENT] Turb check: turb2=%.1f NTU\n", lastTurb2);

    if (lastTurb2 <= TURBIDITY_CLEAN_NTU) {
        Serial.println("[TREATMENT] Water clean — dispensing");
        sol3_set(true);
        pump2_set(true);
        enter_state(TS_DISPENSING);
    } else {
        filterCycles++;
        Serial.printf("[TREATMENT] Turbid — returning for re-filter (cycle %u)\n",
                      filterCycles);
        sol4_set(true);
        pump2_set(true);
        enter_state(TS_RETURNING);
    }
}

static void tick_returning(unsigned long now) {
    if (now - stateEntry >= RETURN_RUN_MS) {
        pump2_set(false);
        sol4_set(false);
        Serial.println("[TREATMENT] Return done — re-filtering");
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FILTERING);
    }
}

static void tick_dispensing(unsigned long now) {
    if (now - stateEntry >= DISPENSE_RUN_MS) {
        pump2_set(false);
        sol3_set(false);
        Serial.println("[TREATMENT] Dispensing done — cooldown");
        enter_state(TS_COOLDOWN);
    }
}

static void tick_cooldown(unsigned long now) {
    if (now - stateEntry >= COOLDOWN_MS) {
        actuator_all_off();   // Defensive cleanup
        Serial.println("[TREATMENT] Cooldown done — IDLE");
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
    filterCycles = 0;
    Serial.println("[TREATMENT] Init — IDLE");
}

void treatment_tick() {
    if (paused) return;

    unsigned long now = millis();

    switch (state) {
    case TS_IDLE:         tick_idle(now);          break;
    case TS_FIRST_FLUSH:  tick_first_flush(now);   break;
    case TS_COLLECTING:   tick_collecting(now);     break;
    case TS_DOSING:       tick_dosing(now);         break;
    case TS_FILTERING:    tick_filtering(now);      break;
    case TS_TURB_CHECK:   tick_turb_check();        break;
    case TS_DISPENSING:   tick_dispensing(now);      break;
    case TS_RETURNING:    tick_returning(now);       break;
    case TS_COOLDOWN:     tick_cooldown(now);        break;
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

uint8_t treatment_filter_cycles() {
    return filterCycles;
}

void treatment_pause() {
    paused = true;
    actuator_all_off();
    Serial.println("[TREATMENT] PAUSED — all actuators OFF");
}

void treatment_resume() {
    paused = false;
    actuator_all_off();
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESUMED — restarting from IDLE");
}

void treatment_reset() {
    paused = false;
    actuator_all_off();
    filterCycles = 0;
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESET — IDLE");
}
