#include "WifiBase.h"
#include <Arduino.h>

class DummySensor : public SHI::Sensor {
public:
  DummySensor() : Sensor("Dummy") {}
  std::shared_ptr<SHI::SensorReadings> readSensor() { return reading; };
  bool setupSensor() {
    SHI::hw.debugSerial->println("Setup Dummy Sensor");
    return true;
  };
  std::shared_ptr<SHI::SensorData> humidity = std::make_shared<SHI::SensorData>(
      new SHI::SensorData({SHI::FLOAT, SHI::FLOAT_TOSTRING, "Humidity"}));
  std::shared_ptr<SHI::SensorData> temperature =
      std::make_shared<SHI::SensorData>(new SHI::SensorData(
          {SHI::FLOAT, SHI::FLOAT_TOSTRING, "Temperature"}));

private:
  std::shared_ptr<SHI::SensorReadings> reading =
      std::make_shared<SHI::SensorReadings>(
          new SHI::SensorReadings({humidity, temperature}));
};

DummySensor dummy;

void setup() {
  SHI::hw.addSensor(dummy);
  SHI::hw.setup("Test");
  dummy.humidity->floatValue = 10.4;
  dummy.temperature->floatValue = 25;
}
void loop() {
  SHI::hw.loop();
  dummy.humidity->floatValue += 1;
  dummy.temperature->floatValue += 2.3;
  delay(1000);
}