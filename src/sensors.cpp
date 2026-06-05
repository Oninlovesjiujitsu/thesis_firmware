#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "config.h"
#include "sensors.h"

static Adafruit_ADS1115 ads;
static bool ads_ok = false;

static float ph_slope  = 0.0f;
static float ph_offset = 0.0f;

// --- Moving-average ring buffers (one per filtered channel) ---
struct MABuffer {
    float buf[MA_WINDOW];
    uint8_t idx;
    uint8_t count;
    float sum;
};

static MABuffer ma_ph    = {};
static MABuffer ma_turb1 = {};
static MABuffer ma_turb2 = {};

struct VoltNTU { float v; float ntu; };


static const VoltNTU TURB_TABLE[] = {
    { 4.10f,    0.0f },
    { 3.70f,  100.0f },
    { 3.60f,  200.0f },
    { 3.40f,  400.0f },
    { 2.50f,  800.0f },
    { 2.00f, 1200.0f },
    { 0.00f, 2000.0f }
};

static const int TURB_TABLE_SIZE = (int)(sizeof(TURB_TABLE) / sizeof(TURB_TABLE[0]));


static float push_moving_average(float v, MABuffer &ma) {
    ma.sum    -= ma.buf[ma.idx];
    ma.buf[ma.idx] = v;
    ma.sum    += v;
    ma.idx     = (ma.idx + 1) % MA_WINDOW;
    if (ma.count < MA_WINDOW) ma.count++;
    return ma.sum / ma.count;
}

static float voltage_to_ntu(float v) {
    if (v >= TURB_TABLE[0].v) return 0.0f;
    if (v <= TURB_TABLE[TURB_TABLE_SIZE - 1].v) return TURB_TABLE[TURB_TABLE_SIZE - 1].ntu;

    for (int i = 0; i < TURB_TABLE_SIZE - 1; i++) {
        if (v <= TURB_TABLE[i].v && v > TURB_TABLE[i + 1].v) {
            float vHi = TURB_TABLE[i].v;
            float vLo = TURB_TABLE[i + 1].v;
            float nHi = TURB_TABLE[i].ntu;
            float nLo = TURB_TABLE[i + 1].ntu;
            float t   = (v - vLo) / (vHi - vLo);   
            return nLo + t * (nHi - nLo);
        }
    }
    return TURB_TABLE[TURB_TABLE_SIZE - 1].ntu; 
}

static float read_median_voltage(uint8_t channel) {
    if (!ads_ok) return 0.0f;

    int16_t samples[MEDIAN_WINDOW];
    for (int i = 0; i < MEDIAN_WINDOW; i++) {
        samples[i] = ads.readADC_SingleEnded(channel);
        delay(SENSOR_SAMPLE_DELAY_MS);
    }

    for (int i = 1; i < MEDIAN_WINDOW; i++) {
        int16_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }

    int16_t med = samples[MEDIAN_WINDOW / 2];
    if (med < 0) med = 0;
    return ads.computeVolts(med);
}

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

static float read_turbidity(uint8_t channel, MABuffer &ma, const char *label) {
    float med_v = read_median_voltage(channel);
    float ma_v  = push_moving_average(med_v, ma);
    float ntu   = voltage_to_ntu(ma_v);

#if DEBUG_SENSORS
    const char *note = "";
    if      (med_v < 0.01f)  note = " (WARNING: 0V — check wiring)";
    else if (med_v >= 3.40f) note = " (CLEAR — < 400 NTU)";
    else if (med_v >= 2.50f) note = " (MODERATE — 400-800 NTU)";
    else if (med_v >= 2.00f) note = " (TURBID — 800-1200 NTU)";
    else                     note = " (OPAQUE — > 1200 NTU)";
    Serial.printf("[%s] med=%.3fV  ma=%.3fV  ntu=%.1f%s\n", label, med_v, ma_v, ntu, note);
#endif

    return ntu;
}

#if CALIBRATION_MODE

static float wait_for_stable_reading(const char *label) {
    Serial.printf("\n[CAL]   Waiting for stable reading in %s buffer...\n", label);
    Serial.println("        Watch the voltage below. When it stops drifting,");
    Serial.println("        type any character in Serial Monitor and press Send.");
    Serial.println();

    float last_v = 0.0f;
    while (true) {
        float v = read_median_voltage(ADS_CH_PH);
        Serial.printf("        Voltage: %.4f V\r", v);
        last_v = v;

        if (Serial.available()) {
            while (Serial.available()) Serial.read();
            Serial.println();
            return last_v;
        }
        delay(500);
    }
}

static void run_calibration() {
    Serial.println("[MODE]  TWO-POINT pH CALIBRATION");
    Serial.println("-------------------------------------------------");
    Serial.println("  You will measure voltage at pH 7.0, then pH 4.0.");
    Serial.println("  Have both buffer solutions ready.");
    Serial.println("  Rinse the probe with distilled water between buffers.");
    Serial.println("-------------------------------------------------");

    Serial.println("\n[STEP 1] Dip the probe in your pH 7.0 buffer.");
    Serial.println("         Wait at least 60 seconds for the probe to stabilise.");
    Serial.println("         Then type any key and press Send in Serial Monitor.");
    float v7 = wait_for_stable_reading("pH 7.0");
    Serial.printf("\n[CAL]   pH 7.0 voltage recorded: %.4f V\n", v7);

    Serial.println("\n[STEP 2] Rinse probe with distilled water.");
    Serial.println("         Dip the probe in your pH 4.0 buffer.");
    Serial.println("         Wait at least 60 seconds, then type any key + Send.");
    float v4 = wait_for_stable_reading("pH 4.0");
    Serial.printf("\n[CAL]   pH 4.0 voltage recorded: %.4f V\n", v4);

    float slope     = (7.0f - 4.0f) / (v7 - v4);
    float intercept = 7.0f - slope * v7;

    Serial.println("\n=================================================");
    Serial.println("  CALIBRATION COMPLETE — copy these values:");
    Serial.println("=================================================");
    Serial.printf("  #define PH_CAL_V_PH7   %.4ff\n", v7);
    Serial.printf("  #define PH_CAL_V_PH4   %.4ff\n", v4);
    Serial.println("-------------------------------------------------");
    Serial.printf("  Derived slope     : %.4f pH/V\n", slope);
    Serial.printf("  Derived intercept : %.4f pH\n",   intercept);
    Serial.println("-------------------------------------------------");
    Serial.println("  Paste into config.h, set CALIBRATION_MODE 0, re-flash.");
    Serial.println("=================================================");

    while (true) delay(1000); 
}

#endif 

void sensors_init() {
    Wire.begin(I2C_SDA, I2C_SCL);

    if (ads.begin(ADS1115_ADDR, &Wire)) {
        ads.setGain(GAIN_TWOTHIRDS);  
        ads_ok = true;
        Serial.println("[SENSORS] ADS1115 initialized OK");
    } else {
        ads_ok = false;
        Serial.println("[SENSORS] ERROR: ADS1115 not found at 0x48");
    }

    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);

    memset(&ma_ph,    0, sizeof(ma_ph));
    memset(&ma_turb1, 0, sizeof(ma_turb1));
    memset(&ma_turb2, 0, sizeof(ma_turb2));

#if CALIBRATION_MODE
    run_calibration();  
#else
    ph_slope  = 3.0f / (PH_CAL_V_PH7 - PH_CAL_V_PH4);
    ph_offset = 7.0f - ph_slope * PH_CAL_V_PH7;

    Serial.printf("[SENSORS] pH cal: V@7=%.4f  V@4=%.4f  slope=%.4f  offset=%.4f\n",
                  PH_CAL_V_PH7, PH_CAL_V_PH4, ph_slope, ph_offset);
    Serial.printf("[SENSORS] Filter: median=%d  MA=%d\n", MEDIAN_WINDOW, MA_WINDOW);
#endif

    // --- Startup diagnostic ---
    Serial.println("\n--- ADS1115 Diagnostic (startup) ---");
    if (ads_ok) {
        static const char *ch_labels[] = {"TURBIDITY1", "PH", "TURBIDITY2", "RAIN"};
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
    float med_v = read_median_voltage(ADS_CH_PH);
    float ma_v  = push_moving_average(med_v, ma_ph);
    float ph    = ph_slope * ma_v + ph_offset;

    if (ph < 0.0f)  ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;

#if DEBUG_SENSORS
    Serial.printf("[pH]    med=%.3fV  ma=%.3fV  ph=%.2f\n", med_v, ma_v, ph);
#endif

    return ph;
}

float sensors_read_turbidity1() {
    return read_turbidity(ADS_CH_TURBIDITY1, ma_turb1, "TURB1");
}

float sensors_read_turbidity2() {
    return read_turbidity(ADS_CH_TURBIDITY2, ma_turb2, "TURB2");
}

float sensors_read_rain() {
    int raw_adc;
    float voltage = read_averaged_voltage(ADS_CH_RAIN, RAIN_SAMPLE_COUNT, &raw_adc);
    float normalized = voltage / RAIN_MAX_V;
    if (normalized > 1.0f) normalized = 1.0f;

#if DEBUG_SENSORS
    Serial.printf("[RAIN]  raw=%d  v=%.3fV  norm=%.3f\n", raw_adc, voltage, normalized);
#endif

    return normalized;
}

bool sensors_read_float_switch() {
    return digitalRead(FLOAT_SWITCH_PIN) == LOW;
}

bool sensors_adc_ok() {
    return ads_ok;
}
