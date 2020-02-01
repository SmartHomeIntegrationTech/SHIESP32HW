#include "WifiBase.h"
#include <Arduino.h>

class DummySensor : public SHI::Sensor {
public:
  virtual std::shared_ptr<SHI::SensorReadings> readSensor() { return reading; };
  virtual bool setupSensor() {
    SHI::hw.debugSerial->println("Setup Dummy Sensor");
    return true;
  };
  virtual String getStatusMessage() { return "OK"; };
  virtual bool errorIsFatal() { return false; };
  virtual String getName() { return "DummySensor"; };
  std::shared_ptr<SHI::SensorData> humidity = std::make_shared<SHI::SensorData>(
      new SHI::SensorData({SHI::FLOAT, SHI::FLOAT_TOSTRING, "Humidity"}));
  std::shared_ptr<SHI::SensorData> temperture =
      std::make_shared<SHI::SensorData>(new SHI::SensorData(
          {SHI::FLOAT, SHI::FLOAT_TOSTRING, "Temperature"}));

private:
  std::shared_ptr<SHI::SensorReadings> reading =
      std::make_shared<SHI::SensorReadings>(
          new SHI::SensorReadings({0, {humidity, temperture}}));
};

DummySensor dummy;

void setup() {
  SHI::hw.addSensor(dummy);
  SHI::hw.setup("Test");
  dummy.humidity->floatValue = 10.4;
  dummy.temperture->floatValue = 25;
}
void loop() {
  SHI::hw.loop();
  dummy.humidity->floatValue += 1;
  dummy.temperture->floatValue += 2.3;
  delay(1000);
}