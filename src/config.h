#ifndef CONFIG_H
#define CONFIG_H

#define ADS1115_ADDR          0x48
#define I2C_SDA               21
#define I2C_SCL               22

#define ADS_CH_TURBIDITY1     0   
#define ADS_CH_TURBIDITY2     2    
#define ADS_CH_PH             1    
#define ADS_CH_RAIN           3    

// Rain sensor max voltage for 0–1 normalization
#define RAIN_MAX_V            3.3f

#define FLOAT_SWITCH_PIN     25   

#define PUMP1_PIN            23   
#define PUMP2_PIN            26   
#define DOSE_ACID_PIN        27   
#define DOSE_BASE_PIN        19   
#define SOL1_PIN             18   
#define SOL2_PIN             17   
#define SOL3_PIN             16  
#define SOL4_PIN              4   

#define MEDIAN_WINDOW          21    
#define MA_WINDOW              20    
#define RAIN_SAMPLE_COUNT      20    
#define SENSOR_SAMPLE_DELAY_MS  5    


// Set to 0 to connect to AWS/IoT Service
#define SERIAL_ONLY_MODE 0

#define DEBUG_SENSORS 1

#define PUBLISH_INTERVAL_MS  5000
#define WIFI_RETRY_MS        5000
#define MQTT_RETRY_MS        5000

#define MQTT_PORT 8883
#define MQTT_TOPIC "thesis/telemetry"
#define MQTT_CLIENT_ID "esp32-wq-001"
#define MQTT_COMMAND_TOPIC "thesis/commands/relay"
#define MQTT_STATUS_TOPIC  "thesis/status/relay"

#define PH_CAL_V_PH7   2.8894f  // Voltage measured at pH 7.0 buffer
#define PH_CAL_V_PH4   3.3824f  // Voltage measured at pH 4.0 buffer

#define CALIBRATION_MODE 0

#define TURBIDITY_CLEAN_NTU  390.0f     

#define RAIN_TRIGGER_THRESHOLD  0.5f    
#define RAIN_DEBOUNCE_MS        3000UL  
#define RAIN_CHECK_INTERVAL_MS  1000UL  
#define FIRST_FLUSH_MS          60000UL 

#define PH_TARGET_MIN          6.5f     
#define PH_TARGET_MAX          7.5f     // Upper bound — stop dosing above this
#define DOSE_PULSE_ON_MS       500UL    
#define DOSE_PULSE_OFF_MS      4500UL  
#define DOSE_MAX_DURATION_MS   120000UL 

#define FLOAT_DEBOUNCE_MS    3000UL    
#define FILTER_TIMEOUT_MS    120000UL  
#define DISPENSE_RUN_MS      120000UL   
#define RETURN_TIMEOUT_MS    120000UL 
#define COOLDOWN_MS          10000UL   
#define MAX_FILTER_CYCLES    2       

#define MQTT_PAYLOAD_BUF_SIZE 384

#endif 
