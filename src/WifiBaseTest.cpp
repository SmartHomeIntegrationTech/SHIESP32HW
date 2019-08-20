#include <Arduino.h>
#include "WifiBase.h"

void setup() {
    wifiDoSetup("Test");
}
void loop() {
    static int count=0;
    if (wifiIsConntected()){
        uploadInfo("StaticIaq", 55);
        uploadInfo("Temperature", 25);
        uploadInfo("Humidity", 60);
        Serial.println("Brightness is now:"+String(count));
        setBrightness(count++);
    }
    feedWatchdog();
    delay(1000);
}