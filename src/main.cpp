#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "network.h"
#include "telemetry.h"

static unsigned long lastPublish = 0;

void setup() {
    Serial.begin(115200);
    sensors_init();
    network_init();
}

void loop() {
    network_mqtt_loop();

    if (!network_ensure_connected()) return;

    unsigned long now = millis();
    if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
        float ph   = sensors_read_ph();
        float turb = sensors_read_turbidity();
        float tank = sensors_read_tank_level();
        telemetry_publish(ph, turb, tank);
        lastPublish = now;
    }
}
