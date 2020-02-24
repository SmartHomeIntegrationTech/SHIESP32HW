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
#include <SHIMQTT.h>
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
    visitor.enterVisit(this);
    visitor.visit(&humidty);
    visitor.visit(&temperature);
    visitor.leaveVisit(this);
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
std::shared_ptr<SHI::SensorGroup> channel =
    std::make_shared<SHI::SensorGroup>("OutsideChannel");
std::shared_ptr<SHI::RestCommunicator> comms =
    std::make_shared<SHI::RestCommunicator>();
std::shared_ptr<SHI::MulticastHandler> multicastComms =
    std::make_shared<SHI::MulticastHandler>();
std::shared_ptr<SHI::MQTT> mqtt = std::make_shared<SHI::MQTT>();
std::shared_ptr<SHI::OLEDDisplay> oled = std::make_shared<SHI::OLEDDisplay>(
    std::pair<String, String>({"OutsideChannelDummyHumidity", "Feuchtigkeit"}),
    std::pair<String, String>(
        {"OutsideChannelDummyTemperature", "Temperatur"}));

class PrintHierachyVisitor : public SHI::Visitor {
 public:
  void enterVisit(SHI::Sensor *sensor) override {
    result +=
        std::string(indent, ' ') + "S:" + std::string(sensor->getName()) + "\n";
    indent++;
  }
  void leaveVisit(SHI::Sensor *sensor) override { indent--; }
  void enterVisit(SHI::SensorGroup *channel) override {
    result += std::string(indent, ' ') +
              "CH:" + std::string(channel->getName()) + "\n";
    indent++;
  }
  void leaveVisit(SHI::SensorGroup *channel) override { indent--; }
  void enterVisit(SHI::Hardware *harwdware) override {
    result += std::string(indent, ' ') +
              "HW:" + std::string(harwdware->getName()) + "\n";
    indent++;
  }
  void leaveVisit(SHI::Hardware *harwdware) override { indent--; }
  void visit(SHI::Communicator *communicator) override {
    result += std::string(indent, ' ') +
              "C:" + std::string(communicator->getName()) + "\n";
  }
  void visit(SHI::MeasurementMetaData *data) override {
    result += std::string(indent, ' ') + "MD:" + data->name +
              " unit:" + data->unit +
              " type:" + DATA_TYPES[static_cast<int>(data->type)] + "\n";
  }

  int indent = 0;
  std::string result = "";

 private:
  const std::string DATA_TYPES[3] = {"INT", "FLOAT", "STRING"};
};

void setup() {
  SHI::hw->addCommunicator(mqtt);
  SHI::hw->addCommunicator(multicastComms);
#ifdef HAS_DISPLAY
  SHI::hw->addCommunicator(oled);
#endif
  oled->setBrightness(5);
  channel->addSensor(dummy);
  SHI::hw->addSensorGroup(channel);
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
