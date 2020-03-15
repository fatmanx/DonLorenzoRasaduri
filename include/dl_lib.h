#ifndef DL_LIB_H
#define DL_LIB_H

#include <Arduino.h>
#include "SPIFFS.h"
#include "FS.h"
#define FORMAT_SPIFFS_IF_FAILED true


//###########################################################################################################
// FILESYSTEM
//###########################################################################################################

bool existsFile(fs::FS &fs, const char *path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        return false;
    }
    return true;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("- failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char *path) {
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while (file.available()) {
        Serial.write(file.read());
    }
}

String readStringFromFile(fs::FS &fs, const char *path) {
    String ret = String();
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("- failed to open file for reading");
        return ret;
    }

    Serial.println("- read from file:");
    while (file.available()) {
        int xx = file.read();
        //    Serial.println(xx);
        ret = ret + (char(xx));

    }
    return ret;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("- failed to open file for writing");
        return;
    }
    if (file.print(message)) {
        Serial.println("- file written");
    } else {
        Serial.println("- frite failed");
    }
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("- failed to open file for appending");
        return;
    }
    if (file.print(message)) {
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char *path) {
    Serial.printf("Deleting file: %s\r\n", path);
    if (fs.remove(path)) {
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}


//###########################################################################################################
// BT MESSAGES
//###########################################################################################################


void startBT()
{
    SerialBT.begin("ESP32 v4");
    Serial.println("SerialBT started");
    setState(STATE_BT_ON, true);
    setState(STATE_WIFI_ON, false);
    setState(STATE_WIFI_CONNECTING, false);
    setState(STATE_BROKER_ON, false);
    setState(STATE_BROKER_CONNECTING, false);
}


void parseMessage(String message) {
    Serial.println(message);
    if (message.indexOf("ssid:") == 0) {
        String ssid = message.substring(5);
        ssid.trim();
        Serial.println(ssid);
        writeFile(SPIFFS, "/ssid", ssid.c_str());
    }
    if (message.indexOf("pass:") == 0) {
        String pass = message.substring(5);
        pass.trim();
        Serial.println(pass);
        writeFile(SPIFFS, "/pass", pass.c_str());
    }
    if (message.indexOf("broker:") == 0) {
        String broker = message.substring(7);
        broker.trim();
        Serial.println(broker);
        writeFile(SPIFFS, "/broker", broker.c_str());
    }
    if (message.indexOf("reset:") == 0) {
        ESP.restart();
    }
}


//###########################################################################################################
// LED CONTROL
//###########################################################################################################

int lastSeqTime = 0;
int seqIndex = 0;
int seqID = -1;

int LEDSequence[][LED_SEQ_CNT] = {
    {1, 1, 1, 1, 1, 0, 0, 0},
    {1, 1, 1, 1, 0, 0, 0, 0},
    {1, 1, 1, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0},

};

void setLed(bool on)
{
    digitalWrite(LED_PIN, on);
}
int isx = 0;
void doLed()
{
    if (isState(STATE_BROKER_ON))
    {
        seqID = 4;
    }
    else if (isState(STATE_BROKER_CONNECTING))
    {
        seqID = 3;
    }
    else if (isState(STATE_WIFI_ON))
    {
        seqID = 2;
    }
    else if (isState(STATE_WIFI_CONNECTING))
    {
        seqID = 1;
    }
    else if (isState(STATE_BT_ON))
    {
        seqID = 0;
    }

    if (millis() - lastSeqTime > LED_SEQ_PERIOD)
    {
        lastSeqTime = millis();
        isx = 1 - isx;
        if (isx)
        {
            setLed(0);
        }
        else
        {
            seqIndex = (seqIndex + 1) % LED_SEQ_CNT;
            setLed(LEDSequence[seqID][seqIndex]);
        }
    }
}

//###########################################################################################################
// STATE CONTROL
//###########################################################################################################


int currentState = 0;

void setState(int state, bool isOn)
{
    if (isOn)
        currentState = currentState | state;
    else
        currentState = currentState & ~state;
}

bool isState(int state)
{
    return (currentState | state) == currentState;
}




#endif //DL_LIB_H