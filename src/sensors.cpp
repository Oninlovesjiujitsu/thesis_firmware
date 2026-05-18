#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "config.h"
#include "sensors.h"

static Adafruit_ADS1115 ads;
static bool ads_ok = false;

// Read averaged voltage from ADS1115 channel (returns volts)
static float read_averaged_voltage(uint8_t channel, int samples) {
    if (!ads_ok) return 0.0f;

    long sum = 0;
    for (int i = 0; i < samples; i++) {
        int16_t raw = ads.readADC_SingleEnded(channel);
        sum += raw;
        delay(SENSOR_SAMPLE_DELAY_MS);
    }
    float avg_raw = sum / (float)samples;
    return ads.computeVolts((int16_t)avg_raw);
}

// Shared turbidity conversion — same calibration curve for both sensors
static float read_turbidity(uint8_t channel, const char *label) {
    float voltage = read_averaged_voltage(channel, SENSOR_SAMPLE_COUNT);
    float ntu = TURB_COEFF_A * voltage * voltage
              + TURB_COEFF_B * voltage
              + TURB_COEFF_C;

#if DEBUG_SENSORS
    Serial.printf("[%s] v=%.3fV  ntu_raw=%.2f", label, voltage, ntu);
#endif

    if (ntu < 0.0f) ntu = 0.0f;
    if (ntu > 3000.0f) ntu = 3000.0f;

#if DEBUG_SENSORS
    Serial.printf("  ntu_clamped=%.2f\n", ntu);
#endif

    return ntu;
}

void sensors_init() {
    Wire.begin(I2C_SDA, I2C_SCL);

    if (ads.begin(ADS1115_ADDR, &Wire)) {
        ads.setGain(GAIN_TWOTHIRDS);  // ±6.144V, 0.1875mV/bit
        ads_ok = true;
        Serial.println("[SENSORS] ADS1115 initialized OK");
    } else {
        ads_ok = false;
        Serial.println("[SENSORS] ERROR: ADS1115 not found at 0x48");
    }

    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);

    // --- Startup diagnostic ---
    Serial.println("\n--- ADS1115 Diagnostic (startup) ---");
    if (ads_ok) {
        static const char *ch_labels[] = {"TURBIDITY1", "TURBIDITY2", "PH", "RAIN"};
        for (int ch = 0; ch < 4; ch++) {
            int16_t raw = ads.readADC_SingleEnded(ch);
            float v = ads.computeVolts(raw);
            Serial.printf("  A%d (%s): raw=%d  v=%.3fV\n", ch, ch_labels[ch], raw, v);
        }
    } else {
        Serial.println("  (skipped — ADS1115 not available)");
    }
    Serial.printf("  GPIO %d (float switch): %s\n", FLOAT_SWITCH_PIN,
                  digitalRead(FLOAT_SWITCH_PIN) == LOW ? "CLOSED (water)" : "OPEN (no water)");
    Serial.println("--- End Diagnostic ---\n");
}

float sensors_read_ph() {
    float voltage = read_averaged_voltage(ADS_CH_PH, SENSOR_SAMPLE_COUNT);
    float ph = (voltage - PH_CAL_V_HIGH) * 14.0f / (PH_CAL_V_LOW - PH_CAL_V_HIGH);

#if DEBUG_SENSORS
    Serial.printf("[pH] v=%.3fV  ph_raw=%.2f", voltage, ph);
#endif

    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;

#if DEBUG_SENSORS
    Serial.printf("  ph_clamped=%.2f\n", ph);
#endif

    return ph;
}

float sensors_read_turbidity1() {
    return read_turbidity(ADS_CH_TURBIDITY1, "TURB1");
}

float sensors_read_turbidity2() {
    return read_turbidity(ADS_CH_TURBIDITY2, "TURB2");
}

float sensors_read_rain() {
    float voltage = read_averaged_voltage(ADS_CH_RAIN, SENSOR_SAMPLE_COUNT);
    float normalized = voltage / RAIN_MAX_V;
    if (normalized > 1.0f) normalized = 1.0f;

#if DEBUG_SENSORS
    Serial.printf("[RAIN] v=%.3fV  normalized=%.3f\n", voltage, normalized);
#endif

    return normalized;
}

bool sensors_read_float_switch() {
    // INPUT_PULLUP: LOW = switch closed = water present
    return digitalRead(FLOAT_SWITCH_PIN) == LOW;
}

bool sensors_adc_ok() {
    return ads_ok;
}
