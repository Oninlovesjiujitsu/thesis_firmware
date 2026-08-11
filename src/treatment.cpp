#include <Arduino.h>
#include "config.h"
#include "treatment.h"
#include "actuator.h"
#include "sensors.h"
#include <PID_v1.h>

static double pidSetpoint = 7.0;
static double pidFakeInput = 7.0;
static double pidOutput = 0.0;

static double pidKp = 2.0;
static double pidKi = 0.5;
static double pidKd = 0.1;

static PID phPID(&pidFakeInput, &pidOutput, &pidSetpoint, pidKp, pidKi, pidKd, DIRECT);

static const unsigned long WindowSize = 5000; 
static unsigned long windowStartTime = 0;

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

static TreatmentState state      = TS_IDLE;
static unsigned long  stateEntry = 0;
static bool           paused     = false;

static unsigned long dosePhaseStart  = 0;   
static unsigned long dosePulseTimer  = 0;   
static bool          dosePulseOn     = false;

static unsigned long debounceStart  = 0;
static bool          debounceActive = false;

static unsigned long lastRainCheck = 0;

static float   lastTurb2      = 0.0f;
static uint8_t filterCycles   = 0;

static void enter_state(TreatmentState next) {
    Serial.printf("[TREATMENT] %s -> %s\n", STATE_NAMES[state], STATE_NAMES[next]);
    state      = next;
    stateEntry = millis();
}


static void tick_idle(unsigned long now) {
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
            filterCycles   = 0;

            dosePhaseStart = now;
            dosePulseTimer = now;
            dosePulseOn    = true;
            windowStartTime = now;
            phPID.SetOutputLimits(0, WindowSize);
            phPID.SetMode(AUTOMATIC);

            Serial.println("[TREATMENT] Tank1 full — dosing (PID-controlled)");
            sol1_set(true);        
            enter_state(TS_DOSING);
        }
    } else {
        debounceActive = false;
    }
}

static void tick_dosing(unsigned long now) {
    if (now - dosePhaseStart >= DOSE_MAX_DURATION_MS) {
        dose_base_set(false);
        dose_acid_set(false);
        Serial.println("[TREATMENT] Dose timeout (safety cap) — filtering");
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FILTERING);
        return;
    }

    float ph = sensors_read_ph();

    if (ph >= PH_TARGET_MIN && ph <= PH_TARGET_MAX) {
        dose_base_set(false);
        dose_acid_set(false);
        Serial.printf("[TREATMENT] pH %.2f in range — filtering\n", ph);
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FILTERING);
        return;
    }

    double error = pidSetpoint - ph;
    bool needsBase = (error > 0);
    
    // Fake the input so PID always treats it as DIRECT action
    pidFakeInput = pidSetpoint - abs(error);
    phPID.Compute();

    // Time proportional control
    if (now - windowStartTime >= WindowSize) {
        windowStartTime += WindowSize;
    }

    if (pidOutput > (now - windowStartTime)) {
        if (needsBase) {
            dose_base_set(true);
            dose_acid_set(false);
        } else {
            dose_acid_set(true);
            dose_base_set(false);
        }
    } else {
        dose_base_set(false);
        dose_acid_set(false);
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
    } else if (filterCycles >= MAX_FILTER_CYCLES) {        
        Serial.println("[TREATMENT] Max filter cycles hit — dispensing anyway"); 
        sol3_set(true);                                   
        pump2_set(true);                                  
        enter_state(TS_DISPENSING);                        
    } else {
        filterCycles++;
        Serial.printf("[TREATMENT] Turbid — returning for re-filter (cycle %u)\n", filterCycles);
        sol4_set(true);
        pump2_set(true);
        enter_state(TS_RETURNING);
    }
}

static void tick_returning(unsigned long now) {
    // Safety cap — if float switch never triggers, proceed anyway
    if (now - stateEntry >= RETURN_TIMEOUT_MS) {
        debounceActive = false;
        pump2_set(false);
        sol4_set(false);
        Serial.println("[TREATMENT] Return timeout — re-filtering (float fault?)");
        sol2_set(true);
        pump1_set(true);
        enter_state(TS_FILTERING);
        return;
    }

    bool full = sensors_read_float_switch();

    if (full) {
        if (!debounceActive) {
            debounceActive = true;
            debounceStart  = now;
        } else if (now - debounceStart >= FLOAT_DEBOUNCE_MS) {
            debounceActive = false;
            pump2_set(false);
            sol4_set(false);
            Serial.println("[TREATMENT] Tank1 full — re-filtering");
            sol2_set(true);
            pump1_set(true);
            enter_state(TS_FILTERING);
        }
    } else {
        debounceActive = false;
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

void treatment_init() {
    state          = TS_IDLE;
    stateEntry     = millis();
    paused         = false;
    filterCycles   = 0;
    dosePhaseStart = 0;   
    dosePulseTimer = 0;   
    dosePulseOn    = false; 
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

void treatment_set_pid(double kp, double ki, double kd, double setpoint) {
    pidKp = kp;
    pidKi = ki;
    pidKd = kd;
    pidSetpoint = setpoint;
    phPID.SetTunings(pidKp, pidKi, pidKd);
    Serial.printf("[TREATMENT] PID Updated: Kp=%.2f Ki=%.2f Kd=%.2f SP=%.2f\n", kp, ki, kd, setpoint);
}
