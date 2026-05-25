#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "config.h"
#include "sensors.h"

static Adafruit_ADS1115 ads;
static bool ads_ok = false;

// Read averaged voltage from ADS1115 channel (returns volts, optionally raw ADC)
static float read_averaged_voltage(uint8_t channel, int samples, int *raw_out = nullptr) {
    if (!ads_ok) {
        if (raw_out) *raw_out = 0;
        return 0.0f;
    }

    long sum = 0;
    for (int i = 0; i < samples; i++) {
        int16_t raw = ads.readADC_SingleEnded(channel);
        sum += raw;
        delay(SENSOR_SAMPLE_DELAY_MS);
    }
    int avg_raw = (int)(sum / (float)samples);
    if (raw_out) *raw_out = avg_raw;
    return ads.computeVolts((int16_t)avg_raw);
}

// Read median voltage from ADS1115 channel (returns volts, optionally raw median)
// DFRobot recommends median filtering for SEN0189 turbidity noise rejection
static float read_median_voltage(uint8_t channel, int samples, int *raw_out = nullptr) {
    if (!ads_ok) {
        if (raw_out) *raw_out = 0;
        return 0.0f;
    }

    int16_t buf[SENSOR_SAMPLE_COUNT];
    for (int i = 0; i < samples; i++) {
        buf[i] = ads.readADC_SingleEnded(channel);
        delay(SENSOR_SAMPLE_DELAY_MS);
    }

    // Insertion sort — 20 elements, trivial cost
    for (int i = 1; i < samples; i++) {
        int16_t key = buf[i];
        int j = i - 1;
        while (j >= 0 && buf[j] > key) {
            buf[j + 1] = buf[j];
            j--;
        }
        buf[j + 1] = key;
    }

    int16_t median = buf[samples / 2];
    if (raw_out) *raw_out = (int)median;
    return ads.computeVolts(median);
}

// Shared turbidity conversion — same calibration curve for both sensors
static float read_turbidity(uint8_t channel, const char *label) {
    int raw_adc;
    float voltage = read_median_voltage(channel, SENSOR_SAMPLE_COUNT, &raw_adc);
    float ntu;

    if (voltage > 4.1f) {
        ntu = 0.0f;          // Pure water
    } else if (voltage < 2.5f) {
        ntu = 3000.0f;       // Max turbidity
    } else {
        ntu = TURB_COEFF_A * voltage * voltage
            + TURB_COEFF_B * voltage
            + TURB_COEFF_C;
    }

    // Quadratic can go slightly negative near boundaries
    if (ntu < 0.0f) ntu = 0.0f;

#if DEBUG_SENSORS
    const char *note = "";
    if (voltage < 0.01f)       note = " (WARNING: 0V — check wiring)";
    else if (voltage > 4.1f)   note = " (CLEAR — voltage above 4.1V threshold)";
    else if (voltage > 3.3f && voltage < 3.7f) note = " (WARNING: possible VDD clamping)";
    else if (voltage < 2.5f)   note = " (OPAQUE — max turbidity range)";
    Serial.printf("[%s] raw=%d  v=%.3fV  ntu=%.2f%s\n", label, raw_adc, voltage, ntu, note);
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
            const char *warn = "";
            if (v < 0.01f)
                warn = "  ** WARNING: No signal — check wiring **";
            else if (v > 3.3f && v < 3.7f && (ch == ADS_CH_TURBIDITY1 || ch == ADS_CH_TURBIDITY2))
                warn = "  ** WARNING: Possible VDD clamping — verify ADS1115 has 5V **";
            Serial.printf("  A%d (%s): raw=%d  v=%.3fV%s\n", ch, ch_labels[ch], raw, v, warn);
        }
    } else {
        Serial.println("  (skipped — ADS1115 not available)");
    }
    Serial.printf("  GPIO %d (float switch): %s\n", FLOAT_SWITCH_PIN,
                  digitalRead(FLOAT_SWITCH_PIN) == LOW ? "CLOSED (water)" : "OPEN (no water)");
    Serial.println("--- End Diagnostic ---\n");
}

float sensors_read_ph() {
    int raw_adc;
    float voltage = read_averaged_voltage(ADS_CH_PH, SENSOR_SAMPLE_COUNT, &raw_adc);
    float ph = (voltage - PH_CAL_V_HIGH) * 14.0f / (PH_CAL_V_LOW - PH_CAL_V_HIGH);

    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;

#if DEBUG_SENSORS
    Serial.printf("[pH]    raw=%d  v=%.3fV  ph=%.2f\n", raw_adc, voltage, ph);
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
    int raw_adc;
    float voltage = read_averaged_voltage(ADS_CH_RAIN, SENSOR_SAMPLE_COUNT, &raw_adc);
    float normalized = voltage / RAIN_MAX_V;
    if (normalized > 1.0f) normalized = 1.0f;

#if DEBUG_SENSORS
    Serial.printf("[RAIN]  raw=%d  v=%.3fV  norm=%.3f\n", raw_adc, voltage, normalized);
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
