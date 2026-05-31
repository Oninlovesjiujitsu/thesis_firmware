#ifndef CONFIG_H
#define CONFIG_H

// --- ADS1115 external ADC (I2C) ---
#define ADS1115_ADDR          0x48
#define I2C_SDA               21
#define I2C_SCL               22

// Channel-to-sensor mapping
#define ADS_CH_TURBIDITY1     0    // A0
#define ADS_CH_TURBIDITY2     2    // A2
#define ADS_CH_PH             1    // A1
#define ADS_CH_RAIN           3    // A3

// Rain sensor max voltage for 0–1 normalization
#define RAIN_MAX_V            3.3f

// Digital sensors on ESP32 GPIO
#define FLOAT_SWITCH_PIN     25   // Digital, INPUT_PULLUP, Tank 1 level (~80% capacity)

// Actuators (all active-low relays)
#define PUMP1_PIN            23   // Tank 1 → filtration → Tank 2
#define PUMP2_PIN            26   // Tank 2 → faucet OR return to Tank 1
#define DOSE_ACID_PIN        27   // Dosing pump 1 (acid, lowers pH)
#define DOSE_BASE_PIN        19   // Dosing pump 2 (base, raises pH)
#define SOL1_PIN             18   // Rain-activated inlet valve
#define SOL2_PIN             17   // Path: Tank 1 → filtration
#define SOL3_PIN             16   // Path: faucet (clean output)
#define SOL4_PIN              4   // Path: return to Tank 1

// --- Sampling ---
#define MEDIAN_WINDOW          21    // odd, for true median (dual-stage filter)
#define MA_WINDOW              20    // moving-average ring buffer depth
#define RAIN_SAMPLE_COUNT      20    // rain still uses simple arithmetic mean
#define SENSOR_SAMPLE_DELAY_MS  5    // ms between ADS reads (ADS1115 blocks until done)

// --- Mode flags ---
// Set to 1 to bypass all network code (WiFi, NTP, TLS, MQTT).
// Sensors read and print to Serial only — use for bench testing.
// Set to 0 to connect to AWS/IoT Service
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
#define MQTT_STATUS_TOPIC  "thesis/status/relay"

// --- pH calibration (two-point, from thesis defense) ---
#define PH_CAL_V_PH7   2.8894f  // Voltage measured at pH 7.0 buffer
#define PH_CAL_V_PH4   3.3824f  // Voltage measured at pH 4.0 buffer

// Set to 1 for serial-guided pH calibration (never returns), 0 for normal operation
#define CALIBRATION_MODE 0

#define TURBIDITY_CLEAN_NTU  390.0f     // Max NTU for drinking water

// --- Rain trigger (HL-83, inverted: low voltage = wet) ---
#define RAIN_TRIGGER_THRESHOLD  0.5f    // Below this = rain detected
#define RAIN_DEBOUNCE_MS        3000UL  // Reject transient moisture
#define RAIN_CHECK_INTERVAL_MS  1000UL  // Throttle rain reads in IDLE
#define FIRST_FLUSH_MS          60000UL // 1 minute first flush duration

// --- Dosing ---
#define DOSE_BASE_FIXED_MS     10000UL  // Fixed 10s base dose after tank full

// --- Treatment timing ---
#define FLOAT_DEBOUNCE_MS    3000UL    // Float switch must be stable 3s
#define FILTER_TIMEOUT_MS    120000UL  // Safety timeout for filtering (2 min)
#define DISPENSE_RUN_MS      120000UL   // Pump 2 run time for dispensing
#define RETURN_TIMEOUT_MS    120000UL  // Safety cap for return fill (matches FILTER_TIMEOUT_MS)
#define COOLDOWN_MS          10000UL   // Motor protection cooldown
#define MAX_FILTER_CYCLES    3         // Max re-filter attempts before FAULT

// --- Payload buffer ---
#define MQTT_PAYLOAD_BUF_SIZE 384

#endif // CONFIG_H  
