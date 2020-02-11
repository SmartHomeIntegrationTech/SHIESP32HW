#include "WifiBase.h"
#include <Arduino.h>
#include <SHIMulticastHandler.h>
#include <SHIOLEDDisplay.h>
#include <SHIRestCommunicator.h>
#include <SHIVisitor.h>

class DummySensor : public SHI::Sensor {
public:
  DummySensor() : Sensor("Dummy") {}

  std::shared_ptr<SHI::SensorReadings> readSensor() {
    SHI::hw.logInfo(name, __func__, "Loop Dummy Sensor");
    return reading;
  }
  bool setupSensor() {
    SHI::hw.logInfo(name, __func__, "Setup Dummy Sensor");
    return true;
  }
  bool stopSensor() {
    SHI::hw.logInfo(name, __func__, "Stop Dummy Sensor");
    return true;
  }
  void accept(SHI::Visitor &visitor) override {
    visitor.visit(this);
    for (auto &&data : reading->data) {
      visitor.visit(&*data);
    }
  };
  std::shared_ptr<SHI::SensorData> humidity =
      std::make_shared<SHI::SensorData>(new SHI::SensorData(
          {SHI::SensorDataType::FLOAT, SHI::FLOAT_TOSTRING, "Humidity", "%"}));
  std::shared_ptr<SHI::SensorData> temperature =
      std::make_shared<SHI::SensorData>(new SHI::SensorData(
          {SHI::SensorDataType::FLOAT, SHI::FLOAT_TOSTRING, "Temperature", "°C"}));

private:
  std::shared_ptr<SHI::SensorReadings> reading =
      std::make_shared<SHI::SensorReadings>(
          new SHI::SensorReadings({humidity, temperature}));
};
std::shared_ptr<DummySensor> dummy = std::make_shared<DummySensor>();
std::shared_ptr<SHI::Channel> channel =
    std::make_shared<SHI::Channel>(dummy, "OutsideChannel");
std::shared_ptr<SHI::RestCommunicator> comms =
    std::make_shared<SHI::RestCommunicator>();
std::shared_ptr<SHI::MulticastHandler> multicastComms =
    std::make_shared<SHI::MulticastHandler>();
std::shared_ptr<SHI::OLEDDisplay> oled =
    std::make_shared<SHI::OLEDDisplay>(
        std::pair<String, String>(
            {"OutsideChannelDummyHumidity", "Feuchtigkeit"}),
        std::pair<String, String>(
            {"OutsideChannelDummyTemperature", "Temperatur"}));

class PrintHierachyVisitor : public SHI::Visitor {
public:
  void visit(SHI::Sensor *sensor) override {
    result += " S:" + sensor->getName() + "\n";
  };
  void visit(SHI::Channel *channel) override {
    result += " CH:" + channel->getName() + "\n";
  };
  void visit(SHI::Hardware *harwdware) override {
    result += harwdware->getName() + "\n";
  };
  void visit(SHI::Communicator *communicator) override {
    result += " C:" + communicator->getName() + "\n";
  };
  void visit(SHI::SensorData *data) override {
    result += "  SD:" + data->name + " unit:"+data->unit+" type:"+String(static_cast<int>(data->type))+"\n";
  }
  String result = "";
};

void setup() {
  // SHI::hw.addCommunicator(comms);
  SHI::hw.addCommunicator(multicastComms);
#ifdef HAS_DISPLAY
  SHI::hw.addCommunicator(oled);
#endif
  oled->setBrightness(5);
  SHI::hw.addSensor(channel);
  SHI::hw.setup("Test");
  dummy->humidity->floatValue = 10.4;
  dummy->temperature->floatValue = 25;
  PrintHierachyVisitor visitor;
  SHI::hw.accept(visitor);
  SHI::hw.logInfo("WifiBaseTest", __func__, visitor.result);
}
void loop() {
  SHI::hw.loop();
  dummy->humidity->floatValue += 1;
  dummy->temperature->floatValue += 2.3;
  delay(1000);
}