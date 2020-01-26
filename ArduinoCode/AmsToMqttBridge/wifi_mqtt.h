#ifndef _WIFI_MQTT_H
#define _WIFI_MQTT_H

#include <MQTTClient.h>

#include "HanConfigAp.h"

void wifi_mqtt_setup(HanConfigAp* ap_config, HardwareSerial* dbg = NULL, MQTTClientCallbackSimple messageCb = NULL);
bool wifi_mqtt_loop();
void wifi_mqtt_publish(String topic, String payload);

#endif//_WIFI_MQTT_H

