#include <Arduino.h>
#include "WifiBase.h"

class DummySensor : public SHI::Sensor {
    public:
        virtual std::shared_ptr<SHI::SensorReadings> readSensor(){
            return reading;
        };
        virtual bool setupSensor() {
            SHI::hw.debugSerial->println("Setup Dummy Sensor");
            return true;
        };
        virtual String getStatusMessage () {
            return "OK";
        };
        virtual bool errorIsFatal() {
            return false;
        };
        virtual String getName() { return "DummyTest"; };
    private:
        std::shared_ptr<SHI::SensorData> humidity=std::make_shared<SHI::SensorData>(new SHI::SensorData({
            SHI::FLOAT, SHI::FLOAT_TOSTRING, "Humidity"
        }));
        std::shared_ptr<SHI::SensorData> temperture=std::make_shared<SHI::SensorData>(new SHI::SensorData({
            SHI::FLOAT, SHI::FLOAT_TOSTRING, "Temperature"
        }));
        std::shared_ptr<SHI::SensorReadings> reading=std::make_shared<SHI::SensorReadings>(new SHI::SensorReadings({
            0, 
            {
                humidity, 
                temperture
            }
            }));
};

DummySensor dummy;

void setup() {
    SHI::hw.addSensor(dummy);
    SHI::hw.setup("Test");
}
void loop() {
    SHI::hw.loop();
    delay(1000);
}