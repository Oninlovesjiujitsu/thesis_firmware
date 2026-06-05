#ifndef NETWORK_H
#define NETWORK_H

#include <PubSubClient.h>

void network_init();
bool network_ensure_connected();
void network_mqtt_loop();
PubSubClient& network_get_mqtt_client();

#endif 
