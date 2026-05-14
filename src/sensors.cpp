#include <Arduino.h>
#include "config.h"
#include "sensors.h"

// Read raw ADC average (no voltage conversion)
static float read_averaged_raw(int pin, int samples) {
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delay(SENSOR_SAMPLE_DELAY_MS);
    }
    return sum / (float)samples;
}

// Shared turbidity conversion — same calibration curve for both sensors
static float read_turbidity(int pin, const char *label) {
    float rawAdc = read_averaged_raw(pin, SENSOR_SAMPLE_COUNT);
    float pinV = rawAdc * (ADC_VREF / ADC_RESOLUTION);
    float sensorV = pinV * TURB_DIVIDER_RATIO;
    float ntu = TURB_COEFF_A * sensorV * sensorV + TURB_COEFF_B * sensorV + TURB_COEFF_C;

#if DEBUG_SENSORS
    Serial.printf("[%s] raw_adc=%.0f  pin_v=%.3fV  sensor_v=%.3fV  ntu_raw=%.2f", label, rawAdc, pinV, sensorV, ntu);
#endif

    if (ntu < 0.0f) ntu = 0.0f;
    if (ntu > 3000.0f) ntu = 3000.0f;

#if DEBUG_SENSORS
    Serial.printf("  ntu_clamped=%.2f\n", ntu);
#endif

    return ntu;
}

void sensors_init() {
    // Explicit ADC config — don't trust framework defaults
    analogSetAttenuation(ADC_11db);  // Full 0–3.3V range
    analogSetWidth(12);              // 12-bit (0–4095)

    pinMode(TURBIDITY1_PIN, INPUT);
    pinMode(TURBIDITY2_PIN, INPUT);      // ADC — no pull-up
    pinMode(PH_PIN, INPUT);
    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);

    // --- ADC Diagnostic: scan all ADC1-safe GPIOs ---
    static const int adc1_pins[] = {32, 33, 34, 35, 36, 39};
    static const int adc1_pin_count = sizeof(adc1_pins) / sizeof(adc1_pins[0]);

    Serial.println("\n--- ADC Diagnostic (startup) ---");
    for (int i = 0; i < adc1_pin_count; i++) {
        int raw = analogRead(adc1_pins[i]);
        const char *tag = "";
        if (adc1_pins[i] == TURBIDITY1_PIN)     tag = "  <- TURBIDITY1";
        else if (adc1_pins[i] == TURBIDITY2_PIN) tag = "  <- TURBIDITY2";
        else if (adc1_pins[i] == PH_PIN)         tag = "  <- PH";
        Serial.printf("  GPIO %d: %4d%s\n", adc1_pins[i], raw, tag);
    }
    Serial.printf("  GPIO %d (float switch): %s\n", FLOAT_SWITCH_PIN,
                  digitalRead(FLOAT_SWITCH_PIN) == LOW ? "CLOSED (water)" : "OPEN (no water)");
    Serial.println("--- End Diagnostic ---\n");
}

float sensors_read_ph() {
    float rawAdc = read_averaged_raw(PH_PIN, SENSOR_SAMPLE_COUNT);
    float pinV = rawAdc * (ADC_VREF / ADC_RESOLUTION);
    float sensorV = pinV * PH_DIVIDER_RATIO;
    float ph = PH_SLOPE * sensorV + PH_OFFSET;

#if DEBUG_SENSORS
    Serial.printf("[pH] raw_adc=%.0f  pin_v=%.3fV  sensor_v=%.3fV  ph_raw=%.2f", rawAdc, pinV, sensorV, ph);
#endif

    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;

#if DEBUG_SENSORS
    Serial.printf("  ph_clamped=%.2f\n", ph);
#endif

    return ph;
}

float sensors_read_turbidity1() {
    return read_turbidity(TURBIDITY1_PIN, "TURB1");
}

float sensors_read_turbidity2() {
    return read_turbidity(TURBIDITY2_PIN, "TURB2");
}

bool sensors_read_float_switch() {
    // INPUT_PULLUP: LOW = switch closed = water present
    return digitalRead(FLOAT_SWITCH_PIN) == LOW;
}
