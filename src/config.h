#ifndef CONFIG_H
#define CONFIG_H

// --- Pin assignments (ADC1 only — ADC2 unavailable when WiFi active) ---
// Sensors
#define TURBIDITY1_PIN       34   // ADC1_CH6, Tank 1 (telemetry only)
#define TURBIDITY2_PIN       32   // ADC1_CH4, Tank 2 (control input)
#define PH_PIN               35   // ADC1_CH7
#define FLOAT_SWITCH_PIN     18   // Digital, INPUT_PULLUP, Tank 1 level
// GPIO 33 reserved for rain sensor — not this task

// Actuators (all active-low relays)
#define PUMP1_PIN            26   // Tank 1 → filtration → Tank 2
#define PUMP2_PIN            25   // Tank 2 → faucet OR return to Tank 1
#define DOSE_ACID_PIN        27   // Dosing pump 1 (acid, lowers pH)
#define DOSE_BASE_PIN        14   // Dosing pump 2 (base, raises pH)
// GPIO 13 reserved for solenoid 1 (first flush) — not this task
#define SOL2_PIN             16   // Path: Tank 1 → filtration
#define SOL3_PIN             17   // Path: faucet (clean output)
#define SOL4_PIN             19   // Path: return to Tank 1

// --- ADC ---
#define ADC_RESOLUTION 4095
#define ADC_VREF 3.3f

// Voltage divider ratios (actual_sensor_V = pin_V × ratio)
// Turbidity sensors output 0–4.5V → need 20k/20k divider (ratio 2.0)
// pH sensor outputs 0–3.0V → fits ADC range directly (ratio 1.0)
#define TURB_DIVIDER_RATIO 2.0f
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

// --- Payload buffer ---
#define MQTT_PAYLOAD_BUF_SIZE 384

#endif // CONFIG_H
