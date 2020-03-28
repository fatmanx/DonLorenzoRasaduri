#include <Arduino.h>

#define MAX_WIFI_RETRIES 7

#define STATE_SEARCH_DS18B20 1
#define STATE_CONFIGURED 2
#define STATE_BT_ON 4
#define STATE_WIFI_CONNECTING 8
#define STATE_WIFI_ON 16
#define STATE_BROKER_CONNECTING 32
#define STATE_BROKER_ON 64

#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
#include <DallasTemperature.h>

BluetoothSerial SerialBT;
char deviceID[13];

void setState(int stateConst, bool isOn);
bool isState(int stateConst);
void messageReceived(String &topic, String &payload);
void connect();
void publishDS18B20(int i);
void publishDHT22(int i);
void doLed();
String getAddr(DeviceAddress addr);