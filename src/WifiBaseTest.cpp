/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include <Arduino.h>
#ifdef IS_BEACON
#include <SHIBLEBeacon.h>
#endif
#include <SHIHardware.h>
#include <SHIMulticastHandler.h>
#include <SHIOLEDDisplay.h>
#include <SHIRestCommunicator.h>
#include <SHIVisitor.h>

class DummySensor : public SHI::Sensor {
 public:
  DummySensor() : Sensor("Dummy") {}

  std::vector<SHI::MeasurementBundle> readSensor() override {
    static int count = 0;
    SHI::hw->logInfo(name, __func__, "Loop Dummy Sensor");
    auto humMeasure = humidty.measuredFloat(humidtyValue);
    SHI::Measurement tempMeasure =
        count++ >= 10 ? temperature.measuredNoData()
                      : temperature.measuredFloat(temperatureValue);
    if (count > 11) count = 0;
    return {SHI::MeasurementBundle({humMeasure, tempMeasure}, this)};
  }
  bool setupSensor() override {
    SHI::hw->logInfo(name, __func__, "Setup Dummy Sensor");
    return true;
  }
  bool stopSensor() override {
    SHI::hw->logInfo(name, __func__, "Stop Dummy Sensor");
    return true;
  }
  void accept(SHI::Visitor &visitor) override {
    visitor.visit(this);
    visitor.visit(&humidty);
    visitor.visit(&temperature);
  }
  SHI::MeasurementMetaData humidty =
      SHI::MeasurementMetaData("Humidity", "%", SHI::SensorDataType::FLOAT);
  SHI::MeasurementMetaData temperature =
      SHI::MeasurementMetaData("Temperature", "°C", SHI::SensorDataType::FLOAT);
  float humidtyValue = 0;
  float temperatureValue = 0;

 private:
};
std::shared_ptr<DummySensor> dummy = std::make_shared<DummySensor>();
std::shared_ptr<SHI::Channel> channel =
    std::make_shared<SHI::Channel>(dummy, "OutsideChannel");
std::shared_ptr<SHI::RestCommunicator> comms =
    std::make_shared<SHI::RestCommunicator>();
std::shared_ptr<SHI::MulticastHandler> multicastComms =
    std::make_shared<SHI::MulticastHandler>();
std::shared_ptr<SHI::OLEDDisplay> oled = std::make_shared<SHI::OLEDDisplay>(
    std::pair<String, String>({"OutsideChannelDummyHumidity", "Feuchtigkeit"}),
    std::pair<String, String>(
        {"OutsideChannelDummyTemperature", "Temperatur"}));

class PrintHierachyVisitor : public SHI::Visitor {
 public:
  void visit(SHI::Sensor *sensor) override {
    result += " S:" + String(sensor->getName()) + "\n";
  };
  void visit(SHI::Channel *channel) override {
    result += " CH:" + String(channel->getName()) + "\n";
  };
  void visit(SHI::Hardware *harwdware) override {
    result += String(harwdware->getName()) + "\n";
  };
  void visit(SHI::Communicator *communicator) override {
    result += " C:" + String(communicator->getName()) + "\n";
  };
  void visit(SHI::MeasurementMetaData *data) override {
    result += String("  SD:") + data->name + " unit:" + data->unit +
              " type:" + String(static_cast<int>(data->type)) + "\n";
  }
  String result = "";
};

void setup() {
  // SHI::hw->addCommunicator(comms);
  SHI::hw->addCommunicator(multicastComms);
#ifdef HAS_DISPLAY
  SHI::hw->addCommunicator(oled);
#endif
  oled->setBrightness(5);
  SHI::hw->addSensor(channel);
  SHI::hw->setup("Test");
  dummy->humidtyValue = 10.4;
  dummy->temperatureValue = 25;
  PrintHierachyVisitor visitor;
  SHI::hw->accept(visitor);
  SHI::hw->logInfo("WifiBaseTest", __func__, visitor.result.c_str());
#ifdef IS_BEACON
  setupBeacon();
#endif
}
void loop() {
  SHI::hw->loop();
  dummy->humidtyValue += 1;
  dummy->temperatureValue += 2.3;
  delay(1000);
}
