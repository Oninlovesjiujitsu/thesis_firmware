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
#define SENSOR_SAMPLE_COUNT 20
#define SENSOR_SAMPLE_DELAY_MS 10

// --- Mode flags ---
// Set to 1 to bypass all network code (WiFi, NTP, TLS, MQTT).
// Sensors read and print to Serial only — use for bench testing.
// Set to 0 to connect to AWS/IoT Service
#define SERIAL_ONLY_MODE 1

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

// --- pH calibration (two-point, measured from DFRobot SEN0161) ---
#define PH_CAL_V_HIGH  3.58f   // Voltage at pH 0 (acid endpoint)
#define PH_CAL_V_LOW   2.08f   // Voltage at pH 14 (base endpoint)

// --- Turbidity calibration (DFRobot SEN0189 quadratic) ---
#define TURB_COEFF_A (-1120.4f)
#define TURB_COEFF_B (5742.3f)
#define TURB_COEFF_C (-4353.8f)

// --- Water quality thresholds (Philippine National Standards) ---
#define TURBIDITY_CLEAN_NTU  5.0f     // Max NTU for drinking water
#define PH_MIN_ACCEPTABLE    6.5f
#define PH_MAX_ACCEPTABLE    8.5f
#define PH_TARGET            7.0f

// --- Rain trigger (HL-83, inverted: low voltage = wet) ---
#define RAIN_TRIGGER_THRESHOLD  0.5f    // Below this = rain detected
#define RAIN_DEBOUNCE_MS        3000UL  // Reject transient moisture
#define RAIN_CHECK_INTERVAL_MS  1000UL  // Throttle rain reads in IDLE
#define FIRST_FLUSH_MS          60000UL // 1 minute first flush duration

// --- Dosing formula (PLACEHOLDER — calibrate via bench titration) ---
#define TANK1_VOLUME_L          18.0f
#define ACID_ML_PER_L_PER_PH   1.0f
#define BASE_ML_PER_L_PER_PH   1.0f
#define DOSE_ACID_RATE_ML_S    2.0f
#define DOSE_BASE_RATE_ML_S    2.0f
#define DOSE_MIN_DURATION_MS   1000UL
#define DOSE_MAX_DURATION_MS   30000UL

// --- Treatment timing ---
#define FLOAT_DEBOUNCE_MS    3000UL    // Float switch must be stable 3s
#define MIX_FORWARD_MS       10000UL   // Sol2+Pump1 mixing duration
#define SETTLE_MS            10000UL   // Settling time after flow stops
#define FILTER_MAX_RUN_MS    10000UL   // Final filter pass duration
#define MAX_DOSE_ATTEMPTS    3         // Max pH correction attempts
#define DISPENSE_RUN_MS      10000UL   // Pump 2 run time for dispensing
#define RETURN_RUN_MS        10000UL   // Pump 2 run time for returning
#define COOLDOWN_MS          10000UL   // Motor protection cooldown
#define MAX_FILTER_CYCLES    3         // Max re-filter attempts before FAULT

// --- Payload buffer ---
#define MQTT_PAYLOAD_BUF_SIZE 384

#endif // CONFIG_H  
