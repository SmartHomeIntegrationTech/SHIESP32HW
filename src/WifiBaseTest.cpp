#include "WifiBase.h"
#include <Arduino.h>
#include <SHIRestCommunicator.h>

class DummySensor : public SHI::Sensor {
public:
  DummySensor() : Sensor("Dummy") {}

  std::shared_ptr<SHI::SensorReadings> readSensor() { return reading; };
  bool setupSensor() {
    SHI::hw.logInfo(name, __func__, "Setup Dummy Sensor");
    return true;
  };
  std::shared_ptr<SHI::SensorData> humidity = std::make_shared<SHI::SensorData>(
      new SHI::SensorData({SHI::SensorDataType::FLOAT, SHI::FLOAT_TOSTRING, "Humidity"}));
  std::shared_ptr<SHI::SensorData> temperature =
      std::make_shared<SHI::SensorData>(new SHI::SensorData(
          {SHI::SensorDataType::FLOAT, SHI::FLOAT_TOSTRING, "Temperature"}));

private:
  std::shared_ptr<SHI::SensorReadings> reading =
      std::make_shared<SHI::SensorReadings>(
          new SHI::SensorReadings({humidity, temperature}));
};
std::shared_ptr<DummySensor> dummy=std::make_shared<DummySensor>();
std::shared_ptr<SHI::Channel> channel=std::make_shared<SHI::Channel>(dummy, "OutsideChannel");
std::shared_ptr<SHI::SHICommunicator> comms=std::make_shared<SHI::SHIRestCommunicator>();

void setup() {
  SHI::hw.addCommunicator(comms);
  SHI::hw.addChannel(channel);
  SHI::hw.setup("Test");
  dummy->humidity->floatValue = 10.4;
  dummy->temperature->floatValue = 25;
}
void loop() {
  SHI::hw.loop();
  dummy->humidity->floatValue += 1;
  dummy->temperature->floatValue += 2.3;
  delay(1000);
}