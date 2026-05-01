#ifndef CONFIG_H
#define CONFIG_H

// --- Pin assignments (ADC1 only — ADC2 unavailable when WiFi active) ---
#define TURBIDITY_PIN 34
#define PH_PIN 35

// --- ADC ---
#define ADC_RESOLUTION 4095
#define ADC_VREF 3.3f
#define VOLTAGE_DIVIDER_RATIO 2.0f

// --- Sampling ---
#define SENSOR_SAMPLE_COUNT 20
#define SENSOR_SAMPLE_DELAY_MS 10

// --- Timing ---
#define PUBLISH_INTERVAL_MS 30000
#define WIFI_RETRY_MS 5000
#define MQTT_RETRY_MS 5000

// --- MQTT ---
#define MQTT_PORT 8883
#define MQTT_TOPIC "thesis/telemetry"
#define MQTT_CLIENT_ID "esp32-wq-001"

// --- pH calibration (DFRobot SEN0161 V1 linear) ---
#define PH_SLOPE (-5.70f)
#define PH_OFFSET (21.34f)

// --- Turbidity calibration (DFRobot SEN0189 quadratic) ---
#define TURB_COEFF_A (-1120.4f)
#define TURB_COEFF_B (5742.3f)
#define TURB_COEFF_C (-4352.9f)

// --- Payload buffer ---
#define MQTT_PAYLOAD_BUF_SIZE 128

// --- Tank level placeholder ---
#define TANK_LEVEL_PLACEHOLDER 100.0f

#endif // CONFIG_H
