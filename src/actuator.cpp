#include <Arduino.h>
#include "config.h"
#include "actuator.h"
#include "treatment.h"

// ---------------------------------------------------------------------------
// Relay shadow — tracks logical state independent of GPIO read
// ---------------------------------------------------------------------------

static bool s_relay_on[RI_COUNT] = {false};

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

static void relay_set(int pin, bool on, const char *label, RelayIdx idx) {
    s_relay_on[idx] = on;
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
    init_relay_pin(SOL1_PIN,      "Sol1");
    init_relay_pin(SOL2_PIN,      "Sol2");
    init_relay_pin(SOL3_PIN,      "Sol3");
    init_relay_pin(SOL4_PIN,      "Sol4");
}

void pump1_set(bool on)      { relay_set(PUMP1_PIN,     on, "Pump1",    RI_PUMP1); }
void pump2_set(bool on)      { relay_set(PUMP2_PIN,     on, "Pump2",    RI_PUMP2); }
void dose_acid_set(bool on)  { relay_set(DOSE_ACID_PIN, on, "DoseAcid", RI_ACID);  }
void dose_base_set(bool on)  { relay_set(DOSE_BASE_PIN, on, "DoseBase", RI_BASE);  }
void sol1_set(bool open)     { relay_set(SOL1_PIN,    open, "Sol1",     RI_SOL1);  }
void sol2_set(bool open)     { relay_set(SOL2_PIN,    open, "Sol2",     RI_SOL2);  }
void sol3_set(bool open)     { relay_set(SOL3_PIN,    open, "Sol3",     RI_SOL3);  }
void sol4_set(bool open)     { relay_set(SOL4_PIN,    open, "Sol4",     RI_SOL4);  }

void actuator_all_off() {
    pump1_set(false);
    pump2_set(false);
    dose_acid_set(false);
    dose_base_set(false);
    sol1_set(false);
    sol2_set(false);
    sol3_set(false);
    sol4_set(false);
    Serial.println("[ACTUATOR] ALL OFF (safety)");
}

bool actuator_relay_state(uint8_t idx) {
    return idx < RI_COUNT ? s_relay_on[idx] : false;
}

CmdResult actuator_handle_command(const char *payload, unsigned int length,
                                   char *relay_out, char *action_out) {
    relay_out[0]  = '\0';
    action_out[0] = '\0';

    if (length > 255) {
        Serial.println("[ACTUATOR] Payload too large, ignored");
        return CMD_NONE;
    }

    if (strstr(payload, "\"PAUSE\"") != NULL) {
        treatment_pause();
        strncpy(action_out, "PAUSE", 8);
        return CMD_OK;
    } else if (strstr(payload, "\"RESUME\"") != NULL) {
        treatment_resume();
        strncpy(action_out, "RESUME", 8);
        return CMD_OK;
    } else if (strstr(payload, "\"RESET\"") != NULL) {
        treatment_reset();
        strncpy(action_out, "RESET", 8);
        return CMD_OK;
    } else if (strstr(payload, "\"ON\"") != NULL || strstr(payload, "\"OFF\"") != NULL) {
        bool on = strstr(payload, "\"ON\"") != NULL;
        strncpy(action_out, on ? "ON" : "OFF", 8);

        // Identify relay first — needed for ACK in both OK and REJECTED cases
        if      (strstr(payload, "\"PUMP1\"")) strncpy(relay_out, "PUMP1", 8);
        else if (strstr(payload, "\"PUMP2\"")) strncpy(relay_out, "PUMP2", 8);
        else if (strstr(payload, "\"SOL1\""))  strncpy(relay_out, "SOL1",  8);
        else if (strstr(payload, "\"SOL2\""))  strncpy(relay_out, "SOL2",  8);
        else if (strstr(payload, "\"SOL3\""))  strncpy(relay_out, "SOL3",  8);
        else if (strstr(payload, "\"SOL4\""))  strncpy(relay_out, "SOL4",  8);
        else if (strstr(payload, "\"ACID\""))  strncpy(relay_out, "ACID",  8);
        else if (strstr(payload, "\"BASE\""))  strncpy(relay_out, "BASE",  8);
        else {
            Serial.println("[ACTUATOR] Unknown relay target");
            return CMD_UNKNOWN_RELAY;
        }

        // Manual relay commands only allowed when treatment is IDLE
        if (treatment_get_state() != TS_IDLE) {
            Serial.println("[ACTUATOR] Manual command rejected — treatment active");
            return CMD_REJECTED_BUSY;
        }

        if      (strcmp(relay_out, "PUMP1") == 0) pump1_set(on);
        else if (strcmp(relay_out, "PUMP2") == 0) pump2_set(on);
        else if (strcmp(relay_out, "SOL1")  == 0) sol1_set(on);
        else if (strcmp(relay_out, "SOL2")  == 0) sol2_set(on);
        else if (strcmp(relay_out, "SOL3")  == 0) sol3_set(on);
        else if (strcmp(relay_out, "SOL4")  == 0) sol4_set(on);
        else if (strcmp(relay_out, "ACID")  == 0) dose_acid_set(on);
        else if (strcmp(relay_out, "BASE")  == 0) dose_base_set(on);

        return CMD_OK;
    } else {
        Serial.println("[ACTUATOR] Unknown command, ignored");
        return CMD_NONE;
    }
}
