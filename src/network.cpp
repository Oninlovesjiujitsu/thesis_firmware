#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "secrets.h"
#include "network.h"
#include "actuator.h"

static WiFiClientSecure tlsClient;
static PubSubClient mqttClient(tlsClient);

static unsigned long lastWifiAttempt = 0;
static unsigned long lastMqttAttempt = 0;

static void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    // PubSubClient payload is NOT null-terminated — copy to stack buffer
    char buf[256];
    unsigned int copyLen = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';

    if (strcmp(topic, MQTT_COMMAND_TOPIC) == 0) {
        char relay_out[8];
        char action_out[8];
        CmdResult res = actuator_handle_command(buf, copyLen, relay_out, action_out);

        if (res != CMD_NONE) {
            char ack[96];
            if (res == CMD_OK) {
                if (relay_out[0] != '\0') {
                    snprintf(ack, sizeof(ack),
                             "{\"status\":\"ok\",\"relay\":\"%s\",\"action\":\"%s\"}",
                             relay_out, action_out);
                } else {
                    snprintf(ack, sizeof(ack),
                             "{\"status\":\"ok\",\"action\":\"%s\"}", action_out);
                }
            } else if (res == CMD_REJECTED_BUSY) {
                snprintf(ack, sizeof(ack),
                         "{\"status\":\"rejected\",\"relay\":\"%s\",\"reason\":\"busy\"}",
                         relay_out);
            } else {
                snprintf(ack, sizeof(ack),
                         "{\"status\":\"rejected\",\"reason\":\"unknown_relay\"}");
            }
            mqttClient.publish(MQTT_STATUS_TOPIC, ack);
        }
    }
}

static void sync_ntp() {
    Serial.print("Syncing NTP...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = 0;
    int retries = 0;
    while (now < 100000 && retries < 20) {
        delay(500);
        time(&now);
        retries++;
    }
    if (now < 100000) {
        Serial.println(" FAILED (TLS will likely fail)");
    } else {
        Serial.println(" OK");
    }
}

static bool connect_wifi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    unsigned long now = millis();
    if (now - lastWifiAttempt < WIFI_RETRY_MS) return false;
    lastWifiAttempt = now;

    Serial.print("WiFi connecting to ");
    Serial.print(SECRET_SSID);
    Serial.print("...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(SECRET_SSID, SECRET_PASS);

    // 30s window — phone hotspot DHCP can be slow
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(" connected, IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println(" failed");
    return false;
}

static bool connect_mqtt() {
    if (mqttClient.connected()) return true;

    unsigned long now = millis();
    if (now - lastMqttAttempt < MQTT_RETRY_MS) return false;
    lastMqttAttempt = now;

    Serial.print("MQTT connecting...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println(" connected");
        mqttClient.subscribe(MQTT_COMMAND_TOPIC);
        Serial.println("Subscribed to " MQTT_COMMAND_TOPIC);
        return true;
    }

    Serial.print(" failed, rc=");
    Serial.println(mqttClient.state());
    return false;
}

void network_init() {
    tlsClient.setCACert(AWS_CERT_CA);
    tlsClient.setCertificate(AWS_CERT_CRT);
    tlsClient.setPrivateKey(AWS_CERT_PRIVATE);

    mqttClient.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);

    while (!connect_wifi()) {
        delay(1000);
    }

    sync_ntp();
}

bool network_ensure_connected() {
    if (!connect_wifi()) return false;
    if (!connect_mqtt()) return false;
    return true;
}

void network_mqtt_loop() {
    if (mqttClient.connected()) {
        mqttClient.loop();
    }
}

PubSubClient& network_get_mqtt_client() {
    return mqttClient;
}
