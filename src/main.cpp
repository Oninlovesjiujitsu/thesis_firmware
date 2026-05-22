#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "network.h"
#include "telemetry.h"
#include "actuator.h"
#include "treatment.h"

static unsigned long lastPublish = 0;

void setup() {
    Serial.begin(115200);
    actuator_init_all();   // First — minimize relay float window
    sensors_init();
    treatment_init();
#if !SERIAL_ONLY_MODE
    network_init();
#endif
}

void loop() {
#if !SERIAL_ONLY_MODE
    network_mqtt_loop();
#endif

    treatment_tick();  // Non-blocking — runs every iteration

    unsigned long now = millis();
    if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
        TelemetryData td;
        td.ph              = sensors_read_ph();
        td.turbidity1      = sensors_read_turbidity1();
        td.turbidity2      = treatment_last_turbidity2();
        td.rain            = sensors_read_rain();
        td.tank1_full      = sensors_read_float_switch();
        td.state           = treatment_state_name();
        td.dose_attempts   = treatment_dose_attempts();
        td.filter_cycles   = treatment_filter_cycles();
        td.ph_at_sensing   = treatment_last_ph();
        td.turb1_at_sensing = treatment_last_turbidity1();
        td.warn_ph_max     = treatment_warn_ph_max();

#if !SERIAL_ONLY_MODE
        if (network_ensure_connected()) {
            telemetry_publish(td);
        }
#else
        // Serial-only: print the payload for bench testing
        char buf[MQTT_PAYLOAD_BUF_SIZE];
        snprintf(buf, sizeof(buf),
                 "{\"ph\":%.2f,\"turb1\":%.2f,\"turb2\":%.2f,"
                 "\"rain\":%.3f,\"tank1\":%s,\"state\":\"%s\","
                 "\"dose\":%u,\"cycles\":%u,"
                 "\"ph_sensed\":%.2f,\"turb1_sensed\":%.2f,\"warn_ph\":%s}",
                 td.ph, td.turbidity1, td.turbidity2,
                 td.rain, td.tank1_full ? "true" : "false",
                 td.state, td.dose_attempts, td.filter_cycles,
                 td.ph_at_sensing, td.turb1_at_sensing,
                 td.warn_ph_max ? "true" : "false");
        Serial.println(buf);
#endif

        lastPublish = now;
    }
}
