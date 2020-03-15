#define LED_PIN 27
#define LED_SEQ_CNT 6
#define LED_SEQ_PERIOD 300

#define ONE_WIRE_BUS 26
#define DS18_NO 3

#define DHT_CNT 3
#define DHTPIN0 32
#define DHTPIN1 33
#define DHTPIN2 25
#define DHTTYPE DHTesp::AM2302

#include "main.h"
#include "dl_lib.h"
#include "FS.h"
#include "SPIFFS.h"
#include "BluetoothSerial.h"

/**
   Control 1-Wire protocol (DS18S20, DS18B20, DS2408 and etc) by Paul Stoffregen
   https://github.com/PaulStoffregen/OneWire
*/
#include <OneWire.h>

// /**
//    Arduino Library for Dallas Temperature ICs (DS18B20, DS18S20, DS1822, DS1820) by milesburton
//    https://github.com/milesburton/Arduino-Temperature-Control-Library.git
// */
#include <DallasTemperature.h>

/**
   DHTesp by beegee tokio
   http://desire.giesecke.tk/index.php/2018/01/30/esp32-dht11/
   https://github.com/beegee-tokyo/DHTesp.git
*/
#include "DHTesp.h"
#include <WiFi.h>

/**
   MQTT library for Arduino by Joel Gaehwiler
   https://github.com/256dpi/arduino-mqtt
*/
#include <MQTT.h>

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress ds18[] = {
    {0x28, 0x3A, 0x4E, 0x94, 0x97, 0x09, 0x03, 0x9A},
    {0x28, 0x99, 0x0F, 0x79, 0x97, 0x19, 0x03, 0x83},
    {0x28, 0x39, 0x02, 0x94, 0x97, 0x10, 0x03, 0x55}};

TempAndHumidity dht22Data;
DHTesp dd[DHT_CNT];
int dhtPins[] = {DHTPIN0, DHTPIN1, DHTPIN2};

String ssid = "";
String pass = "";
String broker_ip = "";
int reportPeriod = 10000;

WiFiClient net;
MQTTClient client;
unsigned long lastMillis = 0;
String message = "";
char incomingChar;

bool doNotRetryWiFi = false;
bool isConf = false;

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    if (!existsFile(SPIFFS, "/ssid") || !existsFile(SPIFFS, "/pass") || !existsFile(SPIFFS, "/broker"))
    {
        Serial.println("No config files");
        startBT();
        return;
    }

    if (existsFile(SPIFFS, "/rep_per"))
    {
        reportPeriod = atoi(readStringFromFile(SPIFFS, "/rep_per").c_str());
        Serial.print("report period:");
        Serial.println(reportPeriod);
    }

    ssid = readStringFromFile(SPIFFS, "/ssid");
    pass = readStringFromFile(SPIFFS, "/pass");
    broker_ip = readStringFromFile(SPIFFS, "/broker");

    isConf = true;

    for (int i = 0; i < DHT_CNT; i++)
    {
        Serial.println(String("Initializing sensor: ") + i + String(" pin:") + String(dhtPins[i]));
        dd[i].setup(dhtPins[i], DHTTYPE);
    }
    Serial.println(F("DHT22 initialized"));
    sensors.begin();
    Serial.println(F("DS18B20 initialized"));
    WiFi.begin(ssid.c_str(), pass.c_str());
    client.begin(broker_ip.c_str(), net);
    client.onMessage(messageReceived);
    connect();
}

void loop()
{
    doLed();

    if (isState(STATE_BT_ON) && SerialBT.isReady() && SerialBT.available())
    {
        char incomingChar = SerialBT.read();
        if (incomingChar != '\n')
        {
            message += String(incomingChar);
        }
        else
        {
            parseMessage(message);
            message = "";
        }
    }

    if (!isConf)
    {
        delay(10);
        return;
    }

    client.loop();
    delay(10); // <- fixes some issues with WiFi stability

    if (!client.connected())
    {
        connect();
    }
    if (millis() - lastMillis > reportPeriod)
    {
        lastMillis = millis();
        for (int i = 0; i < DHT_CNT; i++)
        {
            publishDHT22(i);
        }
        sensors.requestTemperatures();
        for (int i = 0; i < DS18_NO; i++)
        {
            publishDS18B20(i);
        }
    }
}

void publishDS18B20(int i)
{
    float temp = sensors.getTempC(ds18[i]);
    Serial.print("DS18B20 - ");
    Serial.print(i);
    Serial.print(" - ");
    Serial.println(temp);
    client.publish("/DL_temp" + String(i + 3), String(temp));
}

void publishDHT22(int i)
{
    dht22Data = dd[i].getTempAndHumidity();
    Serial.print(i);
    Serial.print(" - ");
    Serial.println("Temp: " + String(dht22Data.temperature, 2) + "'C Humidity: " + String(dht22Data.humidity, 1) + "%");
    client.publish("/DL_temp" + String(i), String(dht22Data.temperature));
    client.publish("/DL_humid" + String(i), String(dht22Data.humidity));
}

void connect()
{
    if (doNotRetryWiFi)
    {
        setState(STATE_WIFI_ON, false);
        setState(STATE_WIFI_CONNECTING, false);
        setState(STATE_BROKER_ON, false);
        setState(STATE_BROKER_CONNECTING, false);
        return;
    }

    Serial.print("checking wifi...");
    int cnt = 0;

    setState(STATE_WIFI_ON, false);
    setState(STATE_WIFI_CONNECTING, true);
    setState(STATE_BROKER_ON, false);
    setState(STATE_BROKER_CONNECTING, false);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
        if (cnt++ > MAX_WIFI_RETRIES)
        {
            doNotRetryWiFi = true;
            Serial.println("WiFi not available");
            startBT();
            return;
        }
    }
    setState(STATE_WIFI_CONNECTING, false);
    setState(STATE_WIFI_ON, true);
    setState(STATE_BROKER_ON, false);
    setState(STATE_BROKER_CONNECTING, true);

    Serial.print("\nconnecting...");
    while (!client.connect("arduino", "try", "try"))
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nconnected!");

    client.subscribe("/setParameterReportPeriod");

    setState(STATE_BROKER_ON, true);
    setState(STATE_BROKER_CONNECTING, false);
}

void messageReceived(String &topic, String &payload)
{
    Serial.println("incoming: " + topic + " - " + payload);
    if (topic == "/setParameterReportPeriod")
    {
        reportPeriod = atoi(payload.c_str());
    }
}
