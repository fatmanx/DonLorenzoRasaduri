#include <Arduino.h>

#define MAX_WIFI_RETRIES 7

#define STATE_CONFIGURED 1
#define STATE_BT_ON 2
#define STATE_WIFI_CONNECTING 4
#define STATE_WIFI_ON 8
#define STATE_BROKER_CONNECTING 16
#define STATE_BROKER_ON 32

#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

void setState(int stateConst, bool isOn);
bool isState(int stateConst);
void messageReceived(String &topic, String &payload);
void connect();
void publishDS18B20(int i);
void publishDHT22(int i);
void doLed();