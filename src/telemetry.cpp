#include <Arduino.h>
#include <pgmspace.h>
#include "config.h"
#include "network.h"
#include "telemetry.h"

static const char PAYLOAD_FMT[] PROGMEM =
    "{\"ph\":%.2f,\"turbidity1\":%.2f,\"turbidity2\":%.2f,"
    "\"rain\":%.3f,\"tank1_full\":%s,\"state\":\"%s\","
    "\"dose_attempts\":%u,\"filter_cycles\":%u,"
    "\"ph_sensed\":%.2f,\"turb1_sensed\":%.2f,\"warn_ph\":%s}";

bool telemetry_publish(const TelemetryData &data) {
    char buf[MQTT_PAYLOAD_BUF_SIZE];
    snprintf_P(buf, sizeof(buf), PAYLOAD_FMT,
               data.ph, data.turbidity1, data.turbidity2,
               data.rain, data.tank1_full ? "true" : "false",
               data.state, data.dose_attempts,
               data.filter_cycles,
               data.ph_at_sensing, data.turb1_at_sensing,
               data.warn_ph_max ? "true" : "false");

    Serial.print("Publishing: ");
    Serial.println(buf);

    bool ok = network_get_mqtt_client().publish(MQTT_TOPIC, buf);
    if (!ok) {
        Serial.println("Publish failed");
    }
    return ok;
}
