/*
 Name:		AmsToMqttBridge.ino
 Created:	3/13/2018 7:40:28 PM
 Author:	roarf

 Reads DLMS/COSEM frames from the HAN port of a Norwegian AMS power meter
 (Kaifa, Aidon, Kamstrup) and publishes them as JSON over MQTT.

 See ArduinoCode/README.md for hardware, wiring and configuration.
*/

#include <esp_ota_ops.h>

#include "config.h"			// Local settings (WiFi, MQTT, OTA). Copy from config.h_

// Bump on every image that gets flashed. Printed at boot and published retained
// on <prefix>/up/version, so "which image is running" can be answered from the
// broker at any time -- the first question every OTA raises.
#define FIRMWARE_VERSION "0x02"

// ---------------------------------------------------------------------------
// Hardware selection
// ---------------------------------------------------------------------------
#define TEMP_SENSOR_NONE	0
#define TEMP_SENSOR_DALLAS	1	// DS18B20 one-wire

#define TEMP_SENSOR TEMP_SENSOR_NONE
#define IS_CUSTOM_AMS_BOARD 0			// 1 only for the ESP8266 PCB designed by Roar Fredriksen

#include <ArduinoJson.h>

#if TEMP_SENSOR == TEMP_SENSOR_DALLAS
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
#define HAN_SERIAL Serial
#else
// Adafruit HUZZAH32 ESP32 Feather + M-BUS to TTL board
#define LED_PIN LED_BUILTIN // Red LED, GPIO13
#define LED_ACTIVE_HIGH 1
#define AP_BUTTON_PIN INVALID_BUTTON_PIN
#define HAN_SERIAL Serial2
#define HAN_RX_PIN 16 // Feather pin labelled "RX", connect to TX on the M-BUS board
#define HAN_TX_PIN 17 // Feather pin labelled "TX", not used by the meter
#endif

#ifndef TEMP_SENSOR_PIN
#define TEMP_SENSOR_PIN 26 // Feather pin labelled "A0"
#endif

#if TEMP_SENSOR == TEMP_SENSOR_DALLAS
static OneWire oneWire(TEMP_SENSOR_PIN);
static DallasTemperature tempSensor(&oneWire);
#endif

// Object used to boot as Access Point
static HanConfigAp ap;

// Object used for debugging
static HardwareSerial* debugger = NULL;

// The HAN Port reader, used to read serial data and decode DLMS
static HanReader hanReader;


#if USE_STATIC_CONFIG
static void loadStaticConfig(configuration& config)
{
	config.ssid               = (char*)WIFI_SSID;
	config.ssidPassword       = (char*)WIFI_PASS;
	config.meterType          = (byte)METER_TYPE;
	config.mqtt               = (char*)MQTT_HOST;
	config.mqttPort           = MQTT_PORT;
	config.mqttClientID       = (char*)"";	// WiFi MAC, which the ACL is scoped to
	config.mqttPublishTopic   = (char*)MQTT_PUBLISH_TOPIC;
	config.mqttSubscribeTopic = (char*)"";	// Nothing to receive; no read grant
	config.mqttUser           = (char*)MQTT_USER;
	config.mqttPass           = (char*)MQTT_PASS;
}
#endif


// Arduino's initArduino() marks a freshly flashed image valid the instant it
// boots, because the weak verifyOta() it consults just returns true. Then the
// bootloader's rollback only catches an image that cannot boot at all -- not
// one that boots and cannot reach the broker, which is what a wrong password
// or host produces. Returning true defers the decision to
// mark_running_image_valid(): any reset before that reverts to the previous
// image.
extern "C" bool verifyRollbackLater()
{
	return true;
}


// Called once the broker connection is up. A USB-flashed image is never
// PENDING_VERIFY, so there is nothing to confirm then. Only asks once.
static void mark_running_image_valid()
{
	static bool done = false;
	if (done) return;
	done = true;

	const esp_partition_t* running = esp_ota_get_running_partition();
	esp_ota_img_states_t   state;

	if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
			state == ESP_OTA_IMG_PENDING_VERIFY)
	{
		esp_ota_mark_app_valid_cancel_rollback();
		if (debugger) debugger->println("OTA image confirmed good, rollback cancelled");
	}
}


static const char* reset_reason_name()
{
	// A panic, a brownout and a clean OTA restart all present as "it
	// rebooted" otherwise, and they want very different responses.
	switch (esp_reset_reason())
	{
		case ESP_RST_POWERON:   return "poweron";
		case ESP_RST_EXT:       return "external";
		case ESP_RST_SW:        return "software";    // ESP.restart(), incl. OTA
		case ESP_RST_PANIC:     return "panic";
		case ESP_RST_INT_WDT:   return "int_wdt";
		case ESP_RST_TASK_WDT:  return "task_wdt";
		case ESP_RST_WDT:       return "wdt";
		case ESP_RST_DEEPSLEEP: return "deepsleep";
		case ESP_RST_BROWNOUT:  return "brownout";    // a tired USB supply
		default:                return "unknown";
	}
}


// Once per boot, as soon as the broker is reachable. Retained, so it answers
// "what is running right now?" with no cable and no lucky timing.
static void announce_boot()
{
	static bool done = false;
	if (done) return;
	done = true;

	const esp_partition_t* running = esp_ota_get_running_partition();
	const char* partition = running ? running->label : "?";

	StaticJsonDocument<192> json;
	json["fw"]        = FIRMWARE_VERSION;
	json["partition"] = partition;
	json["reset"]     = reset_reason_name();
	json["mac"]       = WiFi.macAddress();
	json["ip"]        = WiFi.localIP().toString();

	String msg;
	serializeJson(json, msg);
	wifi_mqtt_publish(wifi_mqtt_prefix() + "/up/version", msg, true);

	if (debugger) debugger->printf("firmware " FIRMWARE_VERSION ", running from %s, reset %s, mac %s\n",
			partition, reset_reason_name(), WiFi.macAddress().c_str());
}


// the setup function runs once when you press reset or power the board
void setup()
{
	// Debug output on the USB serial port. On the custom AMS board the
	// HAN port uses Serial, so leave this NULL there.
#if !IS_CUSTOM_AMS_BOARD
	debugger = &Serial;
#endif

	if (debugger) {
		debugger->begin(115200);
		while (!debugger);
		debugger->println("");
		debugger->println("Started...");
	}

	// Flash the LED, to indicate we can boot as AP now
	pinMode(LED_PIN, OUTPUT);
	led_on();

	delay(1000);

#if USE_STATIC_CONFIG
	loadStaticConfig(ap.config);
	if (debugger) ap.config.print(debugger);
#else
	// Initialize the AP (boots as config access point if no config in EEPROM)
	ap.setup(AP_BUTTON_PIN, debugger);
#endif

	led_off();

	if (!ap.isActivated)
	{
		wifi_mqtt_setup(&ap, debugger, mqttMessageReceived);

		SelfServiceWebServerSetup(&ap.config, debugger, OTA_USER, OTA_PASS);

#if TEMP_SENSOR == TEMP_SENSOR_DALLAS
		tempSensor.begin();
#endif

		// Configure uart for AMS data
#ifdef HAN_RX_PIN
		HAN_SERIAL.begin(2400, SERIAL_8E1, HAN_RX_PIN, HAN_TX_PIN);
#else
		HAN_SERIAL.begin(2400, SERIAL_8E1);
#endif
		while (!HAN_SERIAL);

		hanReader.setup(&HAN_SERIAL, debugger);
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
			// The broker is reachable, so this image is good enough to keep.
			announce_boot();
			mark_running_image_valid();

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

static void addTemperature(JsonObject& data)
{
#if TEMP_SENSOR == TEMP_SENSOR_DALLAS
	tempSensor.requestTemperatures();
	float temp = tempSensor.getTempCByIndex(0);
	if (temp != DEVICE_DISCONNECTED_C) data["temp"] = temp;
#endif
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
		StaticJsonDocument<768> json;

		// Any generic useful info here
		json["id"] = WiFi.macAddress();
		json["IP"] = WiFi.localIP().toString();
		json["up"] = millis();
		json["t"] = time;

		// Add a sub-structure to the json object,
		// to keep the data from the meter itself
		JsonObject data = json.createNestedObject("data");

		addTemperature(data);

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
		if (ap.config.mqttPublishTopic != 0 && strlen(ap.config.mqttPublishTopic) > 0)
		{
			// Publish the json to the MQTT server
			String msg;
			serializeJson(json, msg);

			if (!wifi_mqtt_publish(ap.config.mqttPublishTopic, msg.c_str()) && debugger) {
				debugger->println("MQTT publish failed");
			}
		}

		// Flash LED off
		led_off();
	}
}
