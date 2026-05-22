#include <Arduino.h>
#include "config.h"
#include "treatment.h"
#include "actuator.h"
#include "sensors.h"

// ---------------------------------------------------------------------------
// State names in PROGMEM
// ---------------------------------------------------------------------------
static const char S_IDLE[]         PROGMEM = "IDLE";
static const char S_COLLECTING[]   PROGMEM = "COLLECTING";
static const char S_SENSING[]      PROGMEM = "SENSING";
static const char S_DOSING[]       PROGMEM = "DOSING";
static const char S_MIXING[]       PROGMEM = "MIXING";
static const char S_RETURNING[]    PROGMEM = "RETURNING";
static const char S_SETTLE[]       PROGMEM = "SETTLE";
static const char S_PH_CHECK[]     PROGMEM = "PH_CHECK";
static const char S_FINAL_FILTER[] PROGMEM = "FINAL_FILTER";
static const char S_TURB_CHECK[]   PROGMEM = "TURB_CHECK";
static const char S_DISPENSING[]   PROGMEM = "DISPENSING";
static const char S_COOLDOWN[]     PROGMEM = "COOLDOWN";
static const char S_FAULT[]        PROGMEM = "FAULT";

static const char * const STATE_NAMES[] PROGMEM = {
    S_IDLE, S_COLLECTING, S_SENSING, S_DOSING, S_MIXING,
    S_RETURNING, S_SETTLE, S_PH_CHECK, S_FINAL_FILTER,
    S_TURB_CHECK, S_DISPENSING, S_COOLDOWN, S_FAULT
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

// Sensor readings (retained for telemetry between cycles)
static float   lastTurb1      = 0.0f;
static float   lastTurb2      = 0.0f;
static float   lastPh         = 0.0f;
static uint8_t doseAttempts   = 0;
static uint8_t filterCycles   = 0;
static bool    warnPhMax      = false;

// Context flags for shared states
static bool postFinalFilter      = false;   // SETTLE → TURB_CHECK vs PH_CHECK
static bool returningForRefilter = false;   // RETURNING → FINAL_FILTER vs SETTLE

// Dosing context
static bool         doseUseAcid  = true;
static unsigned long doseDuration = 0;

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

// Calculate dosing pump run time from pH reading.
// Sets *use_acid true for acid, false for base.
// Returns 0 if pH is within acceptable range.
static unsigned long calculate_dose_ms(float ph, bool *use_acid) {
    float delta;
    float ml_per_l_per_ph;
    float pump_rate;

    if (ph > PH_MAX_ACCEPTABLE) {
        *use_acid      = true;
        delta          = ph - PH_TARGET;
        ml_per_l_per_ph = ACID_ML_PER_L_PER_PH;
        pump_rate      = DOSE_ACID_RATE_ML_S;
    } else if (ph < PH_MIN_ACCEPTABLE) {
        *use_acid      = false;
        delta          = PH_TARGET - ph;
        ml_per_l_per_ph = BASE_ML_PER_L_PER_PH;
        pump_rate      = DOSE_BASE_RATE_ML_S;
    } else {
        return 0;  // In range — no dosing needed
    }

    float ml_needed   = delta * ml_per_l_per_ph * TANK1_VOLUME_L;
    unsigned long ms  = (unsigned long)((ml_needed / pump_rate) * 1000.0f);

    // Clamp
    if (ms < DOSE_MIN_DURATION_MS) ms = DOSE_MIN_DURATION_MS;
    if (ms > DOSE_MAX_DURATION_MS) ms = DOSE_MAX_DURATION_MS;

    return ms;
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
            Serial.printf("[TREATMENT] Rain detected (%.3f) — opening Sol1\n", rain);
            sol1_set(true);
            enter_state(TS_COLLECTING);
        }
    } else {
        debounceActive = false;
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
            sol1_set(false);
            Serial.println("[TREATMENT] Tank1 full — closing Sol1, sensing");
            enter_state(TS_SENSING);
        }
    } else {
        debounceActive = false;
    }
}

static void tick_sensing() {
    // One-shot reads (~200ms total with averaging)
    lastPh    = sensors_read_ph();
    lastTurb1 = sensors_read_turbidity1();

    Serial.printf("[TREATMENT] Sensed: pH=%.2f, turb1=%.1f NTU\n", lastPh, lastTurb1);

    // Reset cycle counters
    doseAttempts = 0;
    filterCycles = 0;
    warnPhMax    = false;

    doseDuration = calculate_dose_ms(lastPh, &doseUseAcid);

    if (doseDuration > 0) {
        doseAttempts = 1;
        Serial.printf("[TREATMENT] pH out of range — dosing %s for %lu ms\n",
                      doseUseAcid ? "acid" : "base", doseDuration);
        if (doseUseAcid) dose_acid_set(true);
        else             dose_base_set(true);
        enter_state(TS_DOSING);
    } else {
        Serial.println("[TREATMENT] pH in range — skipping to final filter");
        postFinalFilter = false;
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FINAL_FILTER);
    }
}

static void tick_dosing(unsigned long now) {
    if (now - stateEntry >= doseDuration) {
        dose_acid_set(false);
        dose_base_set(false);
        Serial.println("[TREATMENT] Dose done — mixing");
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_MIXING);
    }
}

static void tick_mixing(unsigned long now) {
    if (now - stateEntry >= MIX_FORWARD_MS) {
        pump1_set(false);
        sol2_set(false);
        Serial.println("[TREATMENT] Mix done — returning T2→T1");
        sol4_set(true);
        pump2_set(true);
        returningForRefilter = false;
        enter_state(TS_RETURNING);
    }
}

static void tick_returning(unsigned long now) {
    if (now - stateEntry >= RETURN_RUN_MS) {
        pump2_set(false);
        sol4_set(false);

        if (returningForRefilter) {
            Serial.println("[TREATMENT] Return done — re-filtering");
            postFinalFilter = false;
            sol2_set(true);
            pump1_set(true);
            enter_state(TS_FINAL_FILTER);
        } else {
            Serial.println("[TREATMENT] Return done — settling");
            postFinalFilter = false;
            enter_state(TS_SETTLE);
        }
    }
}

static void tick_settle(unsigned long now) {
    if (now - stateEntry >= SETTLE_MS) {
        if (postFinalFilter) {
            Serial.println("[TREATMENT] Settle done — turbidity check");
            enter_state(TS_TURB_CHECK);
        } else {
            Serial.println("[TREATMENT] Settle done — pH check");
            enter_state(TS_PH_CHECK);
        }
    }
}

static void tick_ph_check() {
    float ph = sensors_read_ph();
    Serial.printf("[TREATMENT] pH re-check: %.2f\n", ph);

    doseDuration = calculate_dose_ms(ph, &doseUseAcid);

    if (doseDuration == 0) {
        Serial.println("[TREATMENT] pH OK — final filter");
        postFinalFilter = false;
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FINAL_FILTER);
    } else if (doseAttempts >= MAX_DOSE_ATTEMPTS) {
        warnPhMax = true;
        Serial.println("[TREATMENT] pH max attempts — proceeding with warning");
        postFinalFilter = false;
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FINAL_FILTER);
    } else {
        doseAttempts++;
        Serial.printf("[TREATMENT] pH still off — re-dosing %s (attempt %u/%u)\n",
                      doseUseAcid ? "acid" : "base",
                      doseAttempts, (uint8_t)MAX_DOSE_ATTEMPTS);
        if (doseUseAcid) dose_acid_set(true);
        else             dose_base_set(true);
        enter_state(TS_DOSING);
    }
}

static void tick_final_filter(unsigned long now) {
    if (now - stateEntry >= FILTER_MAX_RUN_MS) {
        pump1_set(false);
        sol2_set(false);
        postFinalFilter = true;
        Serial.println("[TREATMENT] Final filter done — settling");
        enter_state(TS_SETTLE);
    }
}

static void tick_turb_check() {
    lastTurb2 = sensors_read_turbidity2();
    Serial.printf("[TREATMENT] Turb check: turb2=%.1f NTU\n", lastTurb2);

    if (lastTurb2 <= TURBIDITY_CLEAN_NTU) {
        Serial.println("[TREATMENT] Water clean — dispensing");
        filterCycles = 0;
        sol3_set(true);
        pump2_set(true);
        enter_state(TS_DISPENSING);
    } else {
        filterCycles++;
        if (filterCycles > MAX_FILTER_CYCLES) {
            enter_fault("Max filter cycles exceeded");
            return;
        }
        Serial.printf("[TREATMENT] Turbid — returning for re-filter (cycle %u/%u)\n",
                      filterCycles, (uint8_t)MAX_FILTER_CYCLES);
        returningForRefilter = true;
        sol4_set(true);
        pump2_set(true);
        enter_state(TS_RETURNING);
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
    doseAttempts = 0;
    filterCycles = 0;
    warnPhMax    = false;
    postFinalFilter      = false;
    returningForRefilter = false;
    Serial.println("[TREATMENT] Init — IDLE");
}

void treatment_tick() {
    if (paused || state == TS_FAULT) return;

    unsigned long now = millis();

    switch (state) {
    case TS_IDLE:         tick_idle(now);          break;
    case TS_COLLECTING:   tick_collecting(now);    break;
    case TS_SENSING:      tick_sensing();          break;
    case TS_DOSING:       tick_dosing(now);        break;
    case TS_MIXING:       tick_mixing(now);        break;
    case TS_RETURNING:    tick_returning(now);     break;
    case TS_SETTLE:       tick_settle(now);        break;
    case TS_PH_CHECK:     tick_ph_check();         break;
    case TS_FINAL_FILTER: tick_final_filter(now);  break;
    case TS_TURB_CHECK:   tick_turb_check();       break;
    case TS_DISPENSING:   tick_dispensing(now);     break;
    case TS_COOLDOWN:     tick_cooldown(now);      break;
    case TS_FAULT:        break;
    }
}

TreatmentState treatment_get_state() {
    return state;
}

const char *treatment_state_name() {
    return STATE_NAMES[state];
}

float treatment_last_turbidity1() {
    return lastTurb1;
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

bool treatment_warn_ph_max() {
    return warnPhMax;
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
    actuator_all_off();
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESUMED — restarting from IDLE");
}

void treatment_reset() {
    paused = false;
    actuator_all_off();
    doseAttempts         = 0;
    filterCycles         = 0;
    warnPhMax            = false;
    postFinalFilter      = false;
    returningForRefilter = false;
    enter_state(TS_IDLE);
    Serial.println("[TREATMENT] RESET — IDLE");
}
