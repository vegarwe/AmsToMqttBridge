#include "wifi_mqtt.h"

#include <HardwareSerial.h>
#include <MQTT.h>
#include <WiFiClientSecure.h>

//#if defined(ESP8266)
//#include <ESP8266WiFi.h>
//#elif defined(ESP32)
//#include <WiFi.h>
//#endif

#include "config.h"

#define WIFI_CONNECTION_TIMEOUT 30000;
#define AWS_MAX_RECONNECT_TRIES 5

const char certificate_pem_crt[] = {"-----BEGIN CERTIFICATE-----\n\
-----END CERTIFICATE-----\n"};

const char private_pem_key[] = {"-----BEGIN RSA PRIVATE KEY-----\n\
-----END RSA PRIVATE KEY-----\n"};


// "AWS root CA1 and C2 (RSA)", see
// https://docs.aws.amazon.com/iot/latest/developerguide/managing-device-certs.html#server-authentication
static auto constexpr aws_root_ca_pem = "-----BEGIN CERTIFICATE-----\n\
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n\
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n\
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n\
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n\
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n\
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n\
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n\
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n\
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n\
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n\
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n\
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n\
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n\
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n\
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n\
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n\
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n\
rqXRfboQnoZsG4q5WTP468SQvvG5\n\
-----END CERTIFICATE-----\n\
-----BEGIN CERTIFICATE-----\n\
MIIFQTCCAymgAwIBAgITBmyf0pY1hp8KD+WGePhbJruKNzANBgkqhkiG9w0BAQwF\n\
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n\
b24gUm9vdCBDQSAyMB4XDTE1MDUyNjAwMDAwMFoXDTQwMDUyNjAwMDAwMFowOTEL\n\
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n\
b3QgQ0EgMjCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK2Wny2cSkxK\n\
gXlRmeyKy2tgURO8TW0G/LAIjd0ZEGrHJgw12MBvIITplLGbhQPDW9tK6Mj4kHbZ\n\
W0/jTOgGNk3Mmqw9DJArktQGGWCsN0R5hYGCrVo34A3MnaZMUnbqQ523BNFQ9lXg\n\
1dKmSYXpN+nKfq5clU1Imj+uIFptiJXZNLhSGkOQsL9sBbm2eLfq0OQ6PBJTYv9K\n\
8nu+NQWpEjTj82R0Yiw9AElaKP4yRLuH3WUnAnE72kr3H9rN9yFVkE8P7K6C4Z9r\n\
2UXTu/Bfh+08LDmG2j/e7HJV63mjrdvdfLC6HM783k81ds8P+HgfajZRRidhW+me\n\
z/CiVX18JYpvL7TFz4QuK/0NURBs+18bvBt+xa47mAExkv8LV/SasrlX6avvDXbR\n\
8O70zoan4G7ptGmh32n2M8ZpLpcTnqWHsFcQgTfJU7O7f/aS0ZzQGPSSbtqDT6Zj\n\
mUyl+17vIWR6IF9sZIUVyzfpYgwLKhbcAS4y2j5L9Z469hdAlO+ekQiG+r5jqFoz\n\
7Mt0Q5X5bGlSNscpb/xVA1wf+5+9R+vnSUeVC06JIglJ4PVhHvG/LopyboBZ/1c6\n\
+XUyo05f7O0oYtlNc/LMgRdg7c3r3NunysV+Ar3yVAhU/bQtCSwXVEqY0VThUWcI\n\
0u1ufm8/0i2BWSlmy5A5lREedCf+3euvAgMBAAGjQjBAMA8GA1UdEwEB/wQFMAMB\n\
Af8wDgYDVR0PAQH/BAQDAgGGMB0GA1UdDgQWBBSwDPBMMPQFWAJI/TPlUq9LhONm\n\
UjANBgkqhkiG9w0BAQwFAAOCAgEAqqiAjw54o+Ci1M3m9Zh6O+oAA7CXDpO8Wqj2\n\
LIxyh6mx/H9z/WNxeKWHWc8w4Q0QshNabYL1auaAn6AFC2jkR2vHat+2/XcycuUY\n\
+gn0oJMsXdKMdYV2ZZAMA3m3MSNjrXiDCYZohMr/+c8mmpJ5581LxedhpxfL86kS\n\
k5Nrp+gvU5LEYFiwzAJRGFuFjWJZY7attN6a+yb3ACfAXVU3dJnJUH/jWS5E4ywl\n\
7uxMMne0nxrpS10gxdr9HIcWxkPo1LsmmkVwXqkLN1PiRnsn/eBG8om3zEK2yygm\n\
btmlyTrIQRNg91CMFa6ybRoVGld45pIq2WWQgj9sAq+uEjonljYE1x2igGOpm/Hl\n\
urR8FLBOybEfdF849lHqm/osohHUqS0nGkWxr7JOcQ3AWEbWaQbLU8uz/mtBzUF+\n\
fUwPfHJ5elnNXkoOrJupmHN5fLT0zLm4BwyydFy4x2+IoZCn9Kr5v2c69BoVYh63\n\
n749sSmvZ6ES8lgQGVMDMBu4Gon2nL2XA46jCfMdiyHxtN/kHNGfZQIG6lzWE7OE\n\
76KlXIx3KadowGuuQNKotOrN8I1LOJwZmhsoVLiJkO/KdYE+HvJkJMcYr07/R54H\n\
9jVlpNMKVv/1F2Rs76giJUmTtt8AF9pYfl3uxRuw0dFfIRDH+fO6AgonB8Xx1sfT\n\
4PsJYGw=\n\
-----END CERTIFICATE-----\n";

// WiFi and MQTT client
static HanConfigAp* ap;
static WiFiClientSecure net;
static MQTTClient mqtt(384);

// Object used for debug output
static HardwareSerial* debugger = NULL;


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


void connectToAWS()
{
    // Try to connect to AWS and count how many times we retried.
    int retries = 0;
    if (debugger) debugger->println("Connecting to AWS IOT");

    String mqttClientID(ap->config.mqttClientID);
    if (mqttClientID.length() == 0)
    {
        mqttClientID = WiFi.macAddress(); // Needs to be unique, so this a good approach
    }

    do
    {
        if (debugger) debugger->print(".");
        delay(200);
        retries++;

        if (ap->config.mqttUser != 0)
        {
            mqtt.connect(mqttClientID.c_str(), ap->config.mqttUser, ap->config.mqttPass);
        }
        else
        {
            mqtt.connect(mqttClientID.c_str());
        }
    } while (!mqtt.connected() && retries < AWS_MAX_RECONNECT_TRIES);

    // Make sure that we did indeed successfully connect to the MQTT broker
    // If not we just end the function and wait for the next loop.
    if (! mqtt.connected())
    {
        if (debugger) debugger->println(" Timeout!");
        return;
    }

    // If we land here, we have successfully connected to AWS!
    // And we can subscribe to topics and send messages.
    if (debugger) debugger->println();
    if (debugger) debugger->println("MQTT Connected");

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

    // Configure WiFiClientSecure to use the AWS certificates we generated
    net.setCACert(aws_root_ca_pem);
    net.setCertificate(certificate_pem_crt);
    net.setPrivateKey(private_pem_key);

    // Setup MQTT
    mqtt.begin(ap->config.mqtt, ap->config.mqttPort, net);
    mqtt.onMessage(messageCb);
    connectToAWS();
}


bool wifi_mqtt_loop()
{
    wifi_loop();
    mqtt.loop();
    delay(10); // <- fixes some issues with WiFi stability

    // Reconnect to WiFi and MQTT as needed
    if (!mqtt.connected()) {
        connectToAWS();
    }

    return mqtt.connected();
}


void wifi_mqtt_publish(String topic, String payload)
{
    mqtt.publish(topic, payload);
}
