#include <Arduino.h>
#include "WifiBase.h"

void setup() {
    wifiDoSetup("Test");
}
void loop() {
    if (wifiIsConntected()){
        uploadInfo("Test", 1.3);
    }
    feedWatchdog();
    delay(5000);
}