/*
 Name:		AmsToMqttBridge.ino
 Created:	3/13/2018 7:40:28 PM
 Author:	roarf
*/


#define HAS_DALLAS_TEMP_SENSOR 1		// Set to zero if Dallas one wire temp sensor is not present
#define IS_CUSTOM_AMS_BOARD 1			// Set to zero if using NodeMCU or board not designed by Roar Fredriksen

#include <ArduinoJson.h>

#if HAS_DALLAS_TEMP_SENSOR
#include <DallasTemperature.h>
#include <OneWire.h>
#endif

#include "wifi_mqtt.h"
#include "HanConfigAp.h"
#include "HanReader.h"
#include "HanToJson.h"
#include "SelfServiceWebServer.h"

#if IS_CUSTOM_AMS_BOARD
#define LED_PIN 2 // The blue on-board LED of the ESP8266 custom AMS board
#define LED_ACTIVE_HIGH 0
#define AP_BUTTON_PIN 0
#define TEMP_SENSOR_PIN 5 // Temperature sensor connected to GPIO5
#else
#define LED_PIN LED_BUILTIN
#define LED_ACTIVE_HIGH 1
#define AP_BUTTON_PIN INVALID_BUTTON_PIN
#endif

#if HAS_DALLAS_TEMP_SENSOR
#ifndef TEMP_SENSOR_PIN
#define TEMP_SENSOR_PIN 26
#endif

static OneWire oneWire(TEMP_SENSOR_PIN);
static DallasTemperature tempSensor(&oneWire);
#endif

// Object used to boot as Access Point
static HanConfigAp ap;

// Object used for debugging
static HardwareSerial* debugger = NULL;

// The HAN Port reader, used to read serial data and decode DLMS
static HanReader hanReader;


// the setup function runs once when you press reset or power the board
void setup()
{
	// Uncomment to debug over the same port as used for HAN communication
	//debugger = &Serial1;

	if (debugger) {
		// Setup serial port for debugging
		debugger->begin(2400, SERIAL_8E1);
		//debugger->begin(115200);
		while (!debugger);
		debugger->println("");
		debugger->println("Started...");
	}

	// Flash the LED, to indicate we can boot as AP now
	pinMode(LED_PIN, OUTPUT);
	led_on();

	delay(1000);

	// Initialize the AP
	ap.setup(AP_BUTTON_PIN, debugger);

	led_off();

	if (!ap.isActivated)
	{
		wifi_mqtt_setup(&ap, debugger, mqttMessageReceived);

		SelfServiceWebServerSetup(&ap.config, debugger);

		// Configure uart for AMS data
		Serial.begin(2400, SERIAL_8E1);
		while (!Serial);

		hanReader.setup(&Serial, debugger);
		// Compensate for the known Kaifa bug
		hanReader.compensateFor09HeaderBug = (ap.config.meterType == 1);
	}
}

// the loop function runs over and over again until power down or reset
void loop()
{
	// Only do normal stuff if we're not booted as AP
	if (!ap.loop())
	{
		led_off();

		bool connected = wifi_mqtt_loop();

		SelfServiceWebServerLoop();

		if (connected)
		{
			readHanPort();
		}
	}
	else
	{
		// Continously flash the LED when AP mode
		if (millis() / 1000 % 2 == 0)   led_on();
		else							led_off();
	}
}


void led_on()
{
#if LED_ACTIVE_HIGH
	digitalWrite(LED_PIN, HIGH);
#else
	digitalWrite(LED_PIN, LOW);
#endif
}


void led_off()
{
#if LED_ACTIVE_HIGH
	digitalWrite(LED_PIN, LOW);
#else
	digitalWrite(LED_PIN, HIGH);
#endif
}


void mqttMessageReceived(String &topic, String &payload)
{

	if (debugger) {
		debugger->println("Incoming MQTT message:");
		debugger->print("[");
		debugger->print(topic);
		debugger->print("] ");
		debugger->println(payload);
	}

	// Do whatever needed here...
	// Ideas could be to query for values or to initiate OTA firmware update
}

void readHanPort()
{
	if (hanReader.read())
	{
		// Flash LED on, this shows us that data is received
		led_on();

		// Get the timestamp (as unix time) from the package
		time_t time = hanReader.getPackageTime();
		if (debugger) debugger->print("Time of the package is: ");
		if (debugger) debugger->println(time);

		// Define a json object to keep the data
		StaticJsonDocument<500> json;

		// Any generic useful info here
		json["id"] = WiFi.macAddress();
		json["IP"] = WiFi.localIP().toString();
		json["up"] = millis();
		json["t"] = time;

		// Add a sub-structure to the json object,
		// to keep the data from the meter itself
		JsonObject data = json.createNestedObject("data");

#if HAS_DALLAS_TEMP_SENSOR
		// Get the temperature too
		tempSensor.requestTemperatures();
		data["temp"] = tempSensor.getTempCByIndex(0);
#endif

		hanToJson(data, ap.config.meterType, hanReader);

		if (json.containsKey("data") && json["data"].containsKey("P")) {
			SelfServiceWebServerActivePower(time, json["data"]["P"].as<int>());
		}

		// Write the json to the debug port
		if (debugger) {
			debugger->print("Sending data to MQTT: ");
			serializeJsonPretty(json, *debugger);
			debugger->println();
		}

		// Make sure we have configured a publish topic
		if (! ap.config.mqttPublishTopic == 0 || strlen(ap.config.mqttPublishTopic) == 0)
		{
			// Publish the json to the MQTT server
			String msg;
			serializeJson(json, msg);

			wifi_mqtt_publish(ap.config.mqttPublishTopic, msg.c_str());
		}

		// Flash LED off
		led_off();
	}
}

