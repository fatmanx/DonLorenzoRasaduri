#define VERSION "Rasaduri V1.0"
#define BUTTON_PIN 15
#define LED_PIN 4
#define LED_SEQ_CNT 8
#define LED_SEQ_PERIOD 200

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
// DeviceAddress ds18[] = {
//     {0x28, 0x3A, 0x4E, 0x94, 0x97, 0x09, 0x03, 0x9A},
//     {0x28, 0x99, 0x0F, 0x79, 0x97, 0x19, 0x03, 0x83},
//     {0x28, 0x39, 0x02, 0x94, 0x97, 0x10, 0x03, 0x55}};

TempAndHumidity dht22Data;
DHTesp dd[DHT_CNT];
int dhtPins[] = {DHTPIN0, DHTPIN1, DHTPIN2};

String ssid = "";
String pass = "";
String broker_ip = "";
int reportPeriod = 1;

WiFiClient net;
MQTTClient client;
unsigned long lastMillis = 0;
String message = "";
char incomingChar;

bool doNotRetryWiFi = false;
bool isConf = false;

char *outStrDS18 = (char *)calloc(64, sizeof(char));
char *outStrDHT22 = (char *)calloc(64, sizeof(char));

int numberOfDevices = 0;
DeviceAddress tempDeviceAddress;
DeviceAddress *deviceAddresses;

String *addrs;

void searchAddresses()
{
    setState(STATE_SEARCH_DS18B20, true);
    while (true)
    {
        if (!sensors.getAddress(tempDeviceAddress, numberOfDevices))
        {
            break;
        }
        numberOfDevices++;
    }

    deviceAddresses = (DeviceAddress *)calloc(numberOfDevices, sizeof(DeviceAddress));

    addrs = (String *)calloc(numberOfDevices, sizeof(String) * 8);
    numberOfDevices = 0;
    while (true)
    {
        if (!sensors.getAddress(tempDeviceAddress, numberOfDevices))
        {
            break;
        }

        for (int i = 0; i < 8; i++)
        {
            *((*(deviceAddresses + numberOfDevices)) + i) = tempDeviceAddress[i];
        }
        addrs[numberOfDevices] = getAddr(tempDeviceAddress);
        numberOfDevices++;
    }
    setState(STATE_SEARCH_DS18B20, false);
}


void setDeviceId()
{
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t chip = (uint16_t)(chipid >> 32);
    snprintf(deviceID, 23, "%04X%08X", chip, (uint32_t)chipid);
}

void blinkLed(int times, int _delay)
{
    for (int i = 0; i < times; i++)
    {
        setLed(1);
        delay(_delay);
        setLed(0);
        delay(_delay);
    }
}

void clearFilesystem()
{
    deleteFile(SPIFFS, "/ssid");
    deleteFile(SPIFFS, "/pass");
    deleteFile(SPIFFS, "/broker");
}

void initializeFilesystem()
{

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
        reportPeriod = max(1, reportPeriod);
    }

    ssid = readStringFromFile(SPIFFS, "/ssid");
    pass = readStringFromFile(SPIFFS, "/pass");
    broker_ip = readStringFromFile(SPIFFS, "/broker");

    isConf = true;
}

void setup()
{

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    Serial.begin(115200);
    Serial.println("Begin");
    setDeviceId();
    Serial.println(deviceID);

    blinkLed(10, 100);

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    if (digitalRead(BUTTON_PIN))
    {
        Serial.println("Clearing filesystem");
        clearFilesystem();
        delay(1000);
        blinkLed(5, 500);
        delay(1000);
        ESP.restart();
    }

    initializeFilesystem();
    //  return;

    sensors.begin();
    searchAddresses();
    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");
    for (int i = 0; i < numberOfDevices; i++)
    {
        Serial.print("Found address: ");
        Serial.println(getAddr(*deviceAddresses + i * 8));
    }
    // Serial.println(addrs[0]);
    // Serial.println(addrs[1]);
    // Serial.println(addrs[2]);
    // Serial.println();
    // Serial.println(getAddr(*deviceAddresses));
    // Serial.println(getAddr(*deviceAddresses + 8));
    // Serial.println(getAddr(*deviceAddresses + 16));

    // return;

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
    delay(500);
    client.publish("/DL_report_period", String(reportPeriod));
}

void loop()
{

    // delay(20);

    doLed();
    //  return;

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
    if (millis() - lastMillis > reportPeriod * 1000)
    {
        lastMillis = millis();
        for (int i = 0; i < DHT_CNT; i++)
        {
            publishDHT22(i);
        }
        for (int i = 0; i < numberOfDevices; i++)
        {
            sensors.requestTemperatures();
            publishDS18B20(i);
        }
        client.publish("/DL_report_period", String(reportPeriod));
    }
}

void publishDS18B20(int i)
{
    float temp = sensors.getTempC(*deviceAddresses + i * 8);
    temp = sensors.getTempC(*deviceAddresses + i * 8);
    Serial.print("DS18B20 - ");
    Serial.print(i);
    Serial.print(" - ");
    Serial.print(getAddr(*deviceAddresses + i * 8));
    Serial.print(" - ");
    Serial.println(temp);

    // client.publish("/DL_temp" + String(i + 3), String(temp));

    sprintf(outStrDS18, "{\"i\":\"%s\", \"t\":%.2f}", getAddr(*deviceAddresses + i * 8).c_str(), temp);
    // Serial.println(outStrDS18);
    client.publish("/DL_temp", outStrDS18);
}

void publishDHT22(int i)
{
    dht22Data = dd[i].getTempAndHumidity();
    Serial.print(i);
    Serial.print(" - ");
    Serial.println("Temp: " + String(dht22Data.temperature, 2) + "'C Humidity: " + String(dht22Data.humidity, 1) + "%");
    // client.publish("/DL_temp" + String(i), String(dht22Data.temperature));
    // client.publish("/DL_humid" + String(i), String(dht22Data.humidity));

    sprintf(outStrDHT22, "{\"i\":\"%d\", \"t\":%.2f, \"h\":%.2f}", i, dht22Data.temperature, dht22Data.humidity);
    // Serial.println(outStrDHT22);
    client.publish("/DL_temp_humid", outStrDHT22);
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
    client.subscribe("/reset");

    setState(STATE_BROKER_ON, true);
    setState(STATE_BROKER_CONNECTING, false);
}

void messageReceived(String &topic, String &payload)
{
    Serial.println("incoming: " + topic + " - " + payload);
    if (topic == "/setParameterReportPeriod")
    {
        reportPeriod = atoi(payload.c_str());
        Serial.println(reportPeriod);
        reportPeriod = max(reportPeriod, 1);

        writeFile(SPIFFS, "/rep_per", payload.c_str());
    }
    else if (topic == "/reset")
    {
        ESP.restart();
    }
}

String getAddr(DeviceAddress addr)
{
    char ret[24] = "";
    sprintf(ret, "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", addr[0], addr[1], addr[3], addr[3], addr[4], addr[5], addr[6], addr[7]);
    return ret;
}
