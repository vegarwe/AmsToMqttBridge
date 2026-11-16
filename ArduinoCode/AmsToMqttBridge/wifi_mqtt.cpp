#include "wifi_mqtt.h"

#include <HardwareSerial.h>
#include <MQTT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

#define WIFI_CONNECTION_TIMEOUT 30000;
#define MQTT_MAX_RECONNECT_TRIES 5

#ifndef MQTT_TLS
#define MQTT_TLS 1
#endif

#if MQTT_TLS
// Broker certificate verified against root_ca_pem from config.h.
static WiFiClientSecure net;
#else
static WiFiClient net;
#endif

// WiFi and MQTT client
static HanConfigAp* ap;
// Buffer must hold topic + full JSON payload (3-phase long list + temperature)
static MQTTClient mqtt(1024);

// Object used for debug output
static HardwareSerial* debugger = NULL;

// MQTT_ROOT "/" <MAC>, e.g. ams/24:62:AB:D2:80:6C. Everything this device says
// about itself goes under <prefix>/up/, which is what the ACL grants.
static String mqttPrefix;


void wifi_loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    // Connect to WiFi access point.
    if (debugger) debugger->print("Connecting to WiFi network: ");
    if (debugger) debugger->println(ap->config.ssid);

    // Make one first attempt at connect, this seems to considerably speed up the first connection
    WiFi.disconnect();
    WiFi.begin(ap->config.ssid, ap->config.ssidPassword);
    delay(1000);

    // Loop (forever...), waiting for the WiFi connection to complete
    long vTimeout = millis() + WIFI_CONNECTION_TIMEOUT;
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        if (debugger) debugger->print(".");

        // If we timed out, disconnect and try again
        if (vTimeout < millis())
        {
            if (debugger)
            {
                debugger->print("Timout during connect. WiFi status is: ");
                debugger->println(WiFi.status());
            }
            WiFi.disconnect();
            WiFi.begin(ap->config.ssid, ap->config.ssidPassword);
            vTimeout = millis() + WIFI_CONNECTION_TIMEOUT;
        }
        yield();
    }

    //// If we still couldn't connect to the WiFi, go to deep sleep for a minute and try again.
    //if(WiFi.status() != WL_CONNECTED){
    //  esp_sleep_enable_timer_wakeup(1 * 60L * 1000000L);
    //  esp_deep_sleep_start();
    //}

    if (debugger) {
        debugger->println();
        debugger->print("WiFi connected, IP address: ");
        debugger->println(WiFi.localIP());
    }
}


void connectMqtt()
{
    // Try to connect to MQTT and count how many times we retried.
    int retries = 0;
    if (debugger) debugger->println("Connecting to MQTT IOT");

    String mqttClientID(ap->config.mqttClientID);
    if (mqttClientID.length() == 0)
    {
        mqttClientID = WiFi.macAddress(); // Needs to be unique, so this a good approach
    }

    // Set before every connect: the broker only accepts a will at CONNECT
    // time. Retained, so a consumer subscribing after the device died can
    // still find out. The ACL must grant write on it, or the broker silently
    // never sends it -- hence <prefix>/up/# there.
    static String willTopic;
    willTopic = mqttPrefix + "/up/online";
    mqtt.setWill(willTopic.c_str(), "offline", true, 0);

    do
    {
        if (debugger) debugger->print(".");
        delay(200);
        retries++;

        if (ap->config.isSecure())
        {
            mqtt.connect(mqttClientID.c_str(), ap->config.mqttUser, ap->config.mqttPass);
        }
        else
        {
            mqtt.connect(mqttClientID.c_str());
        }
    } while (!mqtt.connected() && retries < MQTT_MAX_RECONNECT_TRIES);

    // Make sure that we did indeed successfully connect to the MQTT broker
    // If not we just end the function and wait for the next loop.
    if (! mqtt.connected())
    {
        if (debugger) debugger->println(" Timeout!");
        return;
    }

    // Subscribe to topics and send messages.
    if (debugger) debugger->println();
    if (debugger) debugger->println("MQTT Connected");

    mqtt.publish(mqttPrefix + "/up/online", "online", true, 0);

    // Allow some resources for the WiFi connection
    yield();

    if (ap->config.mqttSubscribeTopic != 0 && strlen(ap->config.mqttSubscribeTopic) > 0)
    {
        mqtt.subscribe(ap->config.mqttSubscribeTopic);
        if (debugger) debugger->printf("  Subscribing to [%s]\r\n", ap->config.mqttSubscribeTopic);
    }
}


void wifi_mqtt_setup(HanConfigAp* ap_config, HardwareSerial* dbg, MQTTClientCallbackSimple messageCb)
{
    ap = ap_config;
    debugger = dbg;

    // Setup Wifi
    WiFi.enableAP(false);
    WiFi.mode(WIFI_STA);
    wifi_loop();

    // Only after WiFi is up: on arduino-esp32 3.x, WiFi.macAddress() returns
    // 00:00:00:00:00:00 until the WiFi driver has started. 0x01 built this
    // before WiFi.mode() and published everything under ams/00:00:00:00:00:00.
    mqttPrefix = String(MQTT_ROOT "/") + WiFi.macAddress();

#if MQTT_TLS
    net.setCACert(root_ca_pem);
#endif

    // Setup MQTT
    mqtt.begin(ap->config.mqtt, ap->config.mqttPort, net);
    mqtt.onMessage(messageCb);
    connectMqtt();
}


bool wifi_mqtt_loop()
{
    wifi_loop();
    mqtt.loop();
    delay(10); // <- fixes some issues with WiFi stability

    // Reconnect to WiFi and MQTT as needed
    if (!mqtt.connected()) {
        connectMqtt();
    }

    return mqtt.connected();
}


bool wifi_mqtt_publish(String topic, String payload, bool retained)
{
    return mqtt.publish(topic, payload, retained, 0);
}


const String& wifi_mqtt_prefix()
{
    return mqttPrefix;
}
