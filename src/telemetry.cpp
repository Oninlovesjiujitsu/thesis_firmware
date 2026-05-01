#include <Arduino.h>
#include <pgmspace.h>
#include "config.h"
#include "network.h"
#include "telemetry.h"

static const char PAYLOAD_FMT[] PROGMEM =
    "{\"ph_level\":%.2f,\"turbidity\":%.2f,\"tank_level\":%.2f}";

bool telemetry_publish(float ph, float turbidity, float tank_level) {
    char buf[MQTT_PAYLOAD_BUF_SIZE];
    snprintf_P(buf, sizeof(buf), PAYLOAD_FMT, ph, turbidity, tank_level);

    Serial.print("Publishing: ");
    Serial.println(buf);

    bool ok = network_get_mqtt_client().publish(MQTT_TOPIC, buf);
    if (!ok) {
        Serial.println("Publish failed");
    }
    return ok;
}
