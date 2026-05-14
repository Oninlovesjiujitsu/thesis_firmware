#include <Arduino.h>
#include "config.h"
#include "actuator.h"
#include "treatment.h"

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static void init_relay_pin(int pin, const char *label) {
    // Pre-load HIGH before setting OUTPUT to prevent active-low relay
    // from briefly energizing during the hi-Z → driven transition.
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT);
    Serial.printf("[ACTUATOR] %s (GPIO %d) init — OFF\n", label, pin);
}

static void relay_set(int pin, bool on, const char *label) {
    // Active-low: LOW = relay ON, HIGH = relay OFF
    digitalWrite(pin, on ? LOW : HIGH);
    Serial.printf("[ACTUATOR] %s %s\n", label, on ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void actuator_init_all() {
    init_relay_pin(PUMP1_PIN,     "Pump1");
    init_relay_pin(PUMP2_PIN,     "Pump2");
    init_relay_pin(DOSE_ACID_PIN, "DoseAcid");
    init_relay_pin(DOSE_BASE_PIN, "DoseBase");
    init_relay_pin(SOL2_PIN,      "Sol2");
    init_relay_pin(SOL3_PIN,      "Sol3");
    init_relay_pin(SOL4_PIN,      "Sol4");
}

void pump1_set(bool on)      { relay_set(PUMP1_PIN,     on, "Pump1"); }
void pump2_set(bool on)      { relay_set(PUMP2_PIN,     on, "Pump2"); }
void dose_acid_set(bool on)  { relay_set(DOSE_ACID_PIN, on, "DoseAcid"); }
void dose_base_set(bool on)  { relay_set(DOSE_BASE_PIN, on, "DoseBase"); }
void sol2_set(bool open)     { relay_set(SOL2_PIN,    open, "Sol2"); }
void sol3_set(bool open)     { relay_set(SOL3_PIN,    open, "Sol3"); }
void sol4_set(bool open)     { relay_set(SOL4_PIN,    open, "Sol4"); }

void actuator_all_off() {
    pump1_set(false);
    pump2_set(false);
    dose_acid_set(false);
    dose_base_set(false);
    sol2_set(false);
    sol3_set(false);
    sol4_set(false);
    Serial.println("[ACTUATOR] ALL OFF (safety)");
}

void actuator_handle_command(const char *payload, unsigned int length) {
    if (length > 64) {
        Serial.println("[ACTUATOR] Payload too large, ignored");
        return;
    }

    if (strstr(payload, "\"PAUSE\"") != NULL) {
        treatment_pause();
    } else if (strstr(payload, "\"RESUME\"") != NULL) {
        treatment_resume();
    } else if (strstr(payload, "\"RESET\"") != NULL) {
        treatment_reset();
    } else if (strstr(payload, "\"ON\"") != NULL || strstr(payload, "\"OFF\"") != NULL) {
        // Manual relay commands only allowed when treatment is IDLE
        if (treatment_get_state() != TS_IDLE) {
            Serial.println("[ACTUATOR] Manual command rejected — treatment active");
            return;
        }
        bool on = strstr(payload, "\"ON\"") != NULL;
        pump1_set(on);
    } else {
        Serial.println("[ACTUATOR] Unknown command, ignored");
    }
}
