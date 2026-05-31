#include <Arduino.h>
#include <pgmspace.h>
#include "config.h"
#include "network.h"
#include "telemetry.h"

static const char PAYLOAD_FMT[] PROGMEM =
    "{\"ph\":%.2f,\"turb1\":%.2f,\"turb2\":%.2f,"
    "\"rain\":%.3f,\"tank1\":%s,\"state\":\"%s\","
    "\"filter_cycles\":%u,"
    "\"relays\":{\"pump1\":%s,\"pump2\":%s,\"acid\":%s,\"base\":%s,"
    "\"sol1\":%s,\"sol2\":%s,\"sol3\":%s,\"sol4\":%s}}";

bool telemetry_publish(const TelemetryData &data) {
    char buf[MQTT_PAYLOAD_BUF_SIZE];
    snprintf_P(buf, sizeof(buf), PAYLOAD_FMT,
               data.ph, data.turbidity1, data.turbidity2,
               data.rain, data.tank1_full ? "true" : "false",
               data.state, data.filter_cycles,
               data.relays[0] ? "true" : "false",
               data.relays[1] ? "true" : "false",
               data.relays[2] ? "true" : "false",
               data.relays[3] ? "true" : "false",
               data.relays[4] ? "true" : "false",
               data.relays[5] ? "true" : "false",
               data.relays[6] ? "true" : "false",
               data.relays[7] ? "true" : "false");

    Serial.print("Publishing: ");
    Serial.println(buf);

    bool ok = network_get_mqtt_client().publish(MQTT_TOPIC, buf);
    if (!ok) {
        Serial.println("Publish failed");
    }
    return ok;
}
