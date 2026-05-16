#ifndef CONFIG_H
#define CONFIG_H

// --- Pin assignments (ADC1 only — ADC2 unavailable when WiFi active) ---
// Sensors
#define TURBIDITY1_PIN       34   // ADC1_CH6, Tank 1
#define TURBIDITY2_PIN       32   // ADC1_CH4, Tank 2 (post-filtration quality check)
#define PH_PIN               35   // ADC1_CH7
#define FLOAT_SWITCH_PIN     25   // Digital, INPUT_PULLUP, Tank 1 level (~80% capacity)
#define RAIN_PIN             33   // ADC1_CH5, HL-83 rain sensor (analog)

// Actuators (all active-low relays)
#define PUMP1_PIN            23   // Tank 1 → filtration → Tank 2
#define PUMP2_PIN            26   // Tank 2 → faucet OR return to Tank 1
#define DOSE_ACID_PIN        27   // Dosing pump 1 (acid, lowers pH)
#define DOSE_BASE_PIN        19   // Dosing pump 2 (base, raises pH)
#define SOL1_PIN             18   // Rain-activated inlet valve
#define SOL2_PIN             17   // Path: Tank 1 → filtration
#define SOL3_PIN             16   // Path: faucet (clean output)
#define SOL4_PIN              4   // Path: return to Tank 1

// --- ADC ---
#define ADC_RESOLUTION 4095
#define ADC_VREF 3.3f

// Voltage divider ratios (actual_sensor_V = pin_V × ratio)
// Turbidity sensors output 0–4.5V → need 20k/10k divider (ratio 3.0)
// pH sensor outputs 0–3.0V → fits ADC range directly (ratio 1.0)
#define TURB_DIVIDER_RATIO 3.0f
#define PH_DIVIDER_RATIO   1.0f

// --- Sampling ---
#define SENSOR_SAMPLE_COUNT 20
#define SENSOR_SAMPLE_DELAY_MS 10

// --- Mode flags ---
// Set to 1 to bypass all network code (WiFi, NTP, TLS, MQTT).
// Sensors read and print to Serial only — use for bench testing.
#define SERIAL_ONLY_MODE 0

// Set to 0 to disable sensor debug output
#define DEBUG_SENSORS 1

// --- Timing ---
#define PUBLISH_INTERVAL_MS  5000
#define WIFI_RETRY_MS        5000
#define MQTT_RETRY_MS        5000

// --- MQTT ---
#define MQTT_PORT 8883
#define MQTT_TOPIC "thesis/telemetry"
#define MQTT_CLIENT_ID "esp32-wq-001"
#define MQTT_COMMAND_TOPIC "thesis/commands/relay"

// --- pH calibration (DFRobot SEN0161 V1 linear) ---
#define PH_SLOPE  (-5.70f)
#define PH_OFFSET (21.34f)

// --- Turbidity calibration (DFRobot SEN0189 quadratic) ---
#define TURB_COEFF_A (-1120.4f)
#define TURB_COEFF_B (5742.3f)
#define TURB_COEFF_C (-4352.9f)

// --- Water quality thresholds ---
#define TURBIDITY_CLEAN_NTU  5.0f
#define PH_MIN               6.5f
#define PH_MAX               8.5f

// --- Treatment timing ---
#define FLOAT_DEBOUNCE_MS    3000UL    // Float switch must be stable 3s
#define FILTER_MAX_RUN_MS    120000UL  // Safety: max filtering time 120s
#define SETTLE_MS            5000UL    // Settling time after flow stops
#define DOSE_PULSE_MS        3000UL    // Dosing pump ON time per attempt
#define DOSE_MIX_MS          30000UL   // Wait for mixing after dose
#define MAX_DOSE_ATTEMPTS    3         // Max pH correction attempts
#define DISPENSE_RUN_MS      60000UL   // Pump 2 run time for dispensing
#define RETURN_RUN_MS        60000UL   // Pump 2 run time for returning
#define COOLDOWN_MS          10000UL   // Motor protection cooldown
#define MAX_FILTER_CYCLES    3         // Max re-filter attempts before FAULT

// --- Payload buffer ---
#define MQTT_PAYLOAD_BUF_SIZE 384

#endif // CONFIG_H
