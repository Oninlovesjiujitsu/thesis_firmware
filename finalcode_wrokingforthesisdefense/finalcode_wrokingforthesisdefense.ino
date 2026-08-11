/**
 * ============================================================
 *  PH-4502C pH Sensor + E201-BNC Electrode
 *  + Gravity SEN0189 Turbidity Sensor
 *  + ADS1115 (16-bit I2C ADC) — ESP32
 *
 *  Board  : DOIT ESP32 DEVKIT V1
 *  ADC    : ADS1115 (16-bit I2C)
 *  Filter : Moving Median → Moving Average (dual-stage, per channel)
 * ============================================================
 *
 *  ABOUT THE PH-4502C:
 *  The PH-4502C module has a built-in op-amp circuit that
 *  amplifies and offsets the E201-BNC electrode signal.
 *  Output: 0–5V analog (PO pin) → pH 0–14
 *  At pH 7.0 → ~2.5V (adjustable via the onboard trimmer pot)
 *  More acidic (lower pH) = higher voltage output.
 *
 *  ABOUT THE SEN0189 (Gravity Turbidity Sensor):
 *  Analog output: 0–4.5V (powered at 5V).
 *  Higher voltage = clearer water (less turbidity).
 *  Lower voltage  = more turbid (cloudy) water.
 *  Turbidity unit: NTU (Nephelometric Turbidity Units).
 *  The DFRobot datasheet gives a piecewise voltage→NTU mapping
 *  that is implemented below as a lookup table with linear
 *  interpolation. Recalibrate for your specific sample range
 *  if high accuracy is required.
 *
 *  TWO-POINT pH CALIBRATION (pH 4.0 and pH 7.0):
 *  Follow the CALIBRATION STEPS below before first use.
 *
 *  CALIBRATION STEPS:
 *  Step 1 — Flash with CALIBRATION_MODE true.
 *  Step 2 — Dip pH probe in pH 7.0 buffer, wait 60 s.
 *            Type any character in Serial Monitor and press Send.
 *  Step 3 — Rinse probe, dip in pH 4.0 buffer, wait 60 s.
 *            Type any character in Serial Monitor and press Send.
 *  Step 4 — Copy the two voltages printed into CAL_V_PH7 and
 *            CAL_V_PH4 below.
 *  Step 5 — Set CALIBRATION_MODE false, re-flash. Done.
 *
 *  WIRING:
 *  ┌──────────────────┬───────────────────┐
 *  │  PH-4502C Pin    │  Connection       │
 *  ├──────────────────┼───────────────────┤
 *  │  VCC (+)         │  5V (ESP32 VIN)   │
 *  │  GND (-)         │  GND              │
 *  │  PO (analog out) │  ADS1115 A0       │
 *  │  BNC connector   │  E201 probe       │
 *  └──────────────────┴───────────────────┘
 *
 *  ┌──────────────────┬───────────────────┐
 *  │  SEN0189 Pin     │  Connection       │
 *  ├──────────────────┼───────────────────┤
 *  │  VCC (+)         │  5V (ESP32 VIN)   │
 *  │  GND (-)         │  GND              │
 *  │  A  (analog out) │  ADS1115 A1       │
 *  └──────────────────┴───────────────────┘
 *
 *  ┌──────────────────┬───────────────────┐
 *  │  ADS1115 Pin     │  ESP32 Pin        │
 *  ├──────────────────┼───────────────────┤
 *  │  VDD             │  5V (ESP32 VIN)   │
 *  │  GND             │  GND              │
 *  │  SCL             │  GPIO 22          │
 *  │  SDA             │  GPIO 21          │
 *  │  ADDR            │  GND (addr 0x48)  │
 *  │  A0              │  PH-4502C PO pin  │
 *  │  A1              │  SEN0189 A pin    │
 *  └──────────────────┴───────────────────┘
 *
 *  NOTE: ADS1115 powered at 5V → accepts 0–5V on A0/A1 directly.
 *        No voltage divider needed. I2C still works at 3.3V logic.
 *
 *  REQUIRED LIBRARY:
 *  Library Manager → search "Adafruit ADS1X15" → Install
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// ─── Calibration mode ───────────────────────────────────────
// true  = guided two-point pH calibration via Serial Monitor
// false = live pH + turbidity readings with filters
#define CALIBRATION_MODE        false

// ─── Two-point pH calibration voltages ──────────────────────
// Fill these in after running CALIBRATION_MODE once:
#define CAL_V_PH7               2.8894f    // voltage measured at pH 7.0 buffer
#define CAL_V_PH4               3.3824f    // voltage measured at pH 4.0 buffer

// ─── ADS1115 configuration ──────────────────────────────────
#define ADS_I2C_ADDRESS         0x48
#define ADS_CH_PH               1          // pH sensor    → A1
#define ADS_CH_TURBIDITY        0          // SEN0189      → A0
// GAIN_TWOTHIRDS: ±6.144V, 0.1875 mV/bit — covers full 0–5V sensor range
#define ADS_GAIN                GAIN_TWOTHIRDS
#define ADS_MV_PER_BIT          0.1875f

// ─── I2C pins ───────────────────────────────────────────────
#define I2C_SDA                 21
#define I2C_SCL                 22

// ─── Filter tuning ──────────────────────────────────────────
#define MEDIAN_WINDOW           21   // raw ADS samples per cycle — must be ODD
#define MA_WINDOW               20   // moving-average depth (per channel)
#define SAMPLE_DELAY_MS         5    // ms between ADS reads
#define READ_INTERVAL           1000 // ms between printed readings

// ─── Objects & state ────────────────────────────────────────
Adafruit_ADS1115 ads;

// pH calibration constants
float phSlope  = 0.0f;
float phOffset = 0.0f;

// Moving-average buffers — one set per channel
float   maPhBuffer[MA_WINDOW];
uint8_t maPhIndex  = 0;
uint8_t maPhCount  = 0;
float   maPhSum    = 0.0f;

float   maTurbBuffer[MA_WINDOW];
uint8_t maTurbIndex  = 0;
uint8_t maTurbCount  = 0;
float   maTurbSum    = 0.0f;

// ────────────────────────────────────────────────────────────
//  SEN0189 VOLTAGE → NTU  (piecewise linear interpolation)
//  Source: DFRobot SEN0189 wiki datasheet table (5V supply)
//  Voltage decreases as turbidity increases.
//  Table: { voltage (V), NTU }  — sorted HIGH to LOW voltage
// ────────────────────────────────────────────────────────────
struct VoltNTU { float v; float ntu; };

// Lookup table derived from DFRobot's published curve
static const VoltNTU turbidityTable[] = {
  { 4.20f,    0.0f },
  { 4.00f,   50.0f },
  { 3.80f,  100.0f },
  { 3.60f,  200.0f },
  { 3.40f,  400.0f },
  { 3.00f,  600.0f },
  { 2.50f,  800.0f },
  { 2.00f, 1000.0f },
  { 0.00f, 1000.0f }   // clamp floor
};
static const int TURB_TABLE_SIZE =
  (int)(sizeof(turbidityTable) / sizeof(turbidityTable[0]));

float voltageToNTU(float v) {
  // Clamp above max voltage → 0 NTU (perfectly clear)
  if (v >= turbidityTable[0].v) return 0.0f;
  // Clamp below min voltage → 1000 NTU
  if (v <= turbidityTable[TURB_TABLE_SIZE - 1].v) return 1000.0f;

  // Find bracketing pair and interpolate
  for (int i = 0; i < TURB_TABLE_SIZE - 1; i++) {
    if (v <= turbidityTable[i].v && v > turbidityTable[i + 1].v) {
      float vHi  = turbidityTable[i].v;
      float vLo  = turbidityTable[i + 1].v;
      float nHi  = turbidityTable[i].ntu;
      float nLo  = turbidityTable[i + 1].ntu;
      float t    = (v - vLo) / (vHi - vLo);  // 0..1
      return nLo + t * (nHi - nLo);
    }
  }
  return 1000.0f; // fallback
}

const char* turbidityStatus(float ntu) {
  if (ntu < 0.0f)    return "SENSOR ERR  ";
  if (ntu < 90.00f)   return "CLEAR       ";
  if (ntu < 100.00f)  return "SLIGHT HAZE ";
  if (ntu < 500.0f)  return "CLOUDY      ";
  if (ntu < 900.0f)  return "VERY TURBID ";
  return                     "OPAQUE      ";
}

// ────────────────────────────────────────────────────────────
//  HELPER: read a single stable median voltage from ADS1115
//          channel — reused for both pH and turbidity
// ────────────────────────────────────────────────────────────
float readMedianVoltage(uint8_t channel) {
  int16_t samples[MEDIAN_WINDOW];

  for (int i = 0; i < MEDIAN_WINDOW; i++) {
    samples[i] = ads.readADC_SingleEnded(channel);
    delay(SAMPLE_DELAY_MS);
  }

  // Insertion sort
  for (int i = 1; i < MEDIAN_WINDOW; i++) {
    int16_t key = samples[i];
    int     j   = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }

  int16_t med = samples[MEDIAN_WINDOW / 2];
  if (med < 0) med = 0;
  return (med * ADS_MV_PER_BIT) / 1000.0f;
}

// ────────────────────────────────────────────────────────────
//  HELPER: push into a moving-average ring buffer
// ────────────────────────────────────────────────────────────
float pushMovingAverage(float v,
                        float* buf,
                        uint8_t& idx,
                        uint8_t& cnt,
                        float&  sum) {
  sum     -= buf[idx];
  buf[idx] = v;
  sum     += v;
  idx      = (idx + 1) % MA_WINDOW;
  if (cnt < MA_WINDOW) cnt++;
  return sum / cnt;
}

// ────────────────────────────────────────────────────────────
//  HELPER: wait for stable calibration reading
// ────────────────────────────────────────────────────────────
float waitForStableReading(const char* label) {
  Serial.printf("\n[CAL]   Waiting for stable reading in %s buffer...\n", label);
  Serial.println("        Watch the voltage below. When it stops drifting,");
  Serial.println("        type any character in Serial Monitor and press Send.");
  Serial.println();

  float lastVoltage = 0.0f;
  while (true) {
    float v = readMedianVoltage(ADS_CH_PH);
    Serial.printf("        Voltage: %.4f V\r", v);
    lastVoltage = v;

    if (Serial.available()) {
      while (Serial.available()) Serial.read();
      Serial.println();
      return lastVoltage;
    }
    delay(500);
  }
}

// ────────────────────────────────────────────────────────────
//  CALIBRATION ROUTINE
// ────────────────────────────────────────────────────────────
void runCalibration() {
  Serial.println("[MODE]  TWO-POINT pH CALIBRATION");
  Serial.println("-------------------------------------------------");
  Serial.println("  You will measure voltage at pH 7.0, then pH 4.0.");
  Serial.println("  Have both buffer solutions ready.");
  Serial.println("  Rinse the probe with distilled water between buffers.");
  Serial.println("-------------------------------------------------");

  Serial.println("\n[STEP 1] Dip the probe in your pH 7.0 buffer.");
  Serial.println("         Wait at least 60 seconds for the probe to stabilise.");
  Serial.println("         Then type any key and press Send in Serial Monitor.");
  float v7 = waitForStableReading("pH 7.0");
  Serial.printf("\n[CAL]   pH 7.0 voltage recorded: %.4f V\n", v7);

  Serial.println("\n[STEP 2] Rinse probe with distilled water.");
  Serial.println("         Dip the probe in your pH 4.0 buffer.");
  Serial.println("         Wait at least 60 seconds, then type any key + Send.");
  float v4 = waitForStableReading("pH 4.0");
  Serial.printf("\n[CAL]   pH 4.0 voltage recorded: %.4f V\n", v4);

  float slope     = (7.0f - 4.0f) / (v7 - v4);
  float intercept = 7.0f - slope * v7;

  Serial.println("\n=================================================");
  Serial.println("  CALIBRATION COMPLETE — copy these values:");
  Serial.println("=================================================");
  Serial.printf("  #define CAL_V_PH7   %.4ff\n", v7);
  Serial.printf("  #define CAL_V_PH4   %.4ff\n", v4);
  Serial.println("-------------------------------------------------");
  Serial.printf("  Derived slope     : %.4f pH/V\n", slope);
  Serial.printf("  Derived intercept : %.4f pH\n",   intercept);
  Serial.println("-------------------------------------------------");
  Serial.println("  Paste CAL_V_PH7 and CAL_V_PH4 into the sketch,");
  Serial.println("  set CALIBRATION_MODE false, then re-flash.");
  Serial.println("=================================================");

  while (true) delay(1000);
}

// ────────────────────────────────────────────────────────────
//  pH conversion — two-point calibrated linear mapping
// ────────────────────────────────────────────────────────────
float voltageToPH(float voltage) {
  return phSlope * voltage + phOffset;
}

const char* phStatus(float ph) {
  if (ph < 0.0f || ph > 14.0f) return "OUT OF RANGE";
  if (ph < 4.0f)               return "STRONG ACID ";
  if (ph < 6.5f)               return "ACIDIC      ";
  if (ph <= 7.5f)              return "NEUTRAL     ";
  if (ph <= 9.0f)              return "ALKALINE    ";
  return                              "STRONG BASE ";
}

// ────────────────────────────────────────────────────────────
//  SETUP
// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("=================================================");
  Serial.println("  PH-4502C + E201-BNC + SEN0189 + ADS1115 — ESP32");
  Serial.println("  Filter : Moving Median + Moving Average (per ch)");
  Serial.println("=================================================");

  if (!ads.begin(ADS_I2C_ADDRESS)) {
    Serial.println("[ERROR] ADS1115 not found on I2C bus.");
    Serial.println("        Check: SDA→21, SCL→22, VDD→5V, ADDR→GND");
    while (true) delay(1000);
  }

  ads.setGain(ADS_GAIN);
  Serial.printf("[INFO]  ADS1115 ready at 0x%02X\n", ADS_I2C_ADDRESS);
  Serial.printf("[INFO]  Gain: GAIN_TWOTHIRDS (±6.144V, %.4f mV/bit)\n",
                ADS_MV_PER_BIT);
  Serial.printf("[INFO]  pH sensor     → A%d\n", ADS_CH_PH);
  Serial.printf("[INFO]  SEN0189 Turb  → A%d\n", ADS_CH_TURBIDITY);

  // Zero out moving-average buffers
  for (int i = 0; i < MA_WINDOW; i++) {
    maPhBuffer[i]   = 0.0f;
    maTurbBuffer[i] = 0.0f;
  }

#if CALIBRATION_MODE
  runCalibration();   // never returns
#else
  // Validate pH calibration constants
  if (CAL_V_PH7 < 0.001f || CAL_V_PH4 < 0.001f) {
    Serial.println("[ERROR] pH calibration voltages not set.");
    Serial.println("        Run with CALIBRATION_MODE true first.");
    while (true) delay(1000);
  }

  if (abs(CAL_V_PH7 - CAL_V_PH4) < 0.05f) {
    Serial.println("[ERROR] CAL_V_PH7 and CAL_V_PH4 are too close.");
    Serial.println("        Recalibrate with fresh buffer solutions.");
    while (true) delay(1000);
  }

  // Two-point linear calibration: pH = slope * V + intercept
  phSlope  = (7.0f - 4.0f) / (CAL_V_PH7 - CAL_V_PH4);
  phOffset = 7.0f - phSlope * CAL_V_PH7;

  Serial.printf("[INFO]  Cal pH 7.0   : %.4f V\n",   CAL_V_PH7);
  Serial.printf("[INFO]  Cal pH 4.0   : %.4f V\n",   CAL_V_PH4);
  Serial.printf("[INFO]  pH Slope     : %.4f pH/V\n", phSlope);
  Serial.printf("[INFO]  pH Intercept : %.4f pH\n",   phOffset);
  Serial.printf("[INFO]  Median window: %d samples\n", MEDIAN_WINDOW);
  Serial.printf("[INFO]  MA window    : %d values\n",  MA_WINDOW);
  Serial.println("-------------------------------------------------");
  Serial.println(
    "  Time(ms)  |  pH V(med) |  pH V(ma) |   pH  | pH Status   "
    "|| Turb V(med) | Turb V(ma) |  NTU   | Turbidity"
  );
  Serial.println(
    "-------------------------------------------------"
    "----------------------------------------------------"
  );
#endif
}

// ────────────────────────────────────────────────────────────
//  LOOP
// ────────────────────────────────────────────────────────────
void loop() {
  // ── pH sensor (A0) ──────────────────────────────────────
  float phMedianV = readMedianVoltage(ADS_CH_PH);
  float phMaV     = pushMovingAverage(phMedianV,
                                      maPhBuffer,
                                      maPhIndex,
                                      maPhCount,
                                      maPhSum);
  float ph        = voltageToPH(phMaV);

  // ── Turbidity sensor (A1) ────────────────────────────────
  float turbMedianV = readMedianVoltage(ADS_CH_TURBIDITY);
  float turbMaV     = pushMovingAverage(turbMedianV,
                                        maTurbBuffer,
                                        maTurbIndex,
                                        maTurbCount,
                                        maTurbSum);
  float ntu         = voltageToNTU(turbMaV);

  // ── Print combined reading ───────────────────────────────
  Serial.printf(
    "  %9lu | %10.4f | %9.4f | %5.2f | %s || %11.4f | %10.4f | %6.1f | %s\n",
    millis(),
    phMedianV,  phMaV,  ph,  phStatus(ph),
    turbMedianV, turbMaV, ntu, turbidityStatus(ntu)
  );

  delay(READ_INTERVAL);
}
