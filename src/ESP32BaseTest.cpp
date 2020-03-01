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
#ifdef HAS_DISPLAY
#include <SHISDS1306OLED.h>
#endif
#include <SHIRestCommunicator.h>
#include <SHIVisitor.h>

#include "SHIESP32HW.h"
#include "SHIESP32HW_config.h"

class DummySensor : public SHI::Sensor {
 public:
  DummySensor() : Sensor("Dummy") {}

  std::vector<SHI::MeasurementBundle> readSensor() override {
    static int count = 0;
    SHI::hw->logInfo(name, __func__, "Loop Dummy Sensor");
    auto humMeasure = humidty->measuredFloat(humidtyValue);
    SHI::Measurement tempMeasure =
        count++ >= 10 ? temperature->measuredNoData()
                      : temperature->measuredFloat(temperatureValue);
    if (count > 11) count = 0;
    return {
        SHI::MeasurementBundle({humMeasure, tempMeasure, getStatus()}, this)};
  }
  bool setupSensor() override {
    SHI::hw->logInfo(name, __func__, "Setup Dummy Sensor");
    addMetaData(humidty);
    addMetaData(temperature);
    return true;
  }
  bool stopSensor() override {
    SHI::hw->logInfo(name, __func__, "Stop Dummy Sensor");
    return true;
  }
  std::shared_ptr<SHI::MeasurementMetaData> humidty =
      std::make_shared<SHI::MeasurementMetaData>("Humidity", "%",
                                                 SHI::SensorDataType::FLOAT);
  std::shared_ptr<SHI::MeasurementMetaData> temperature =
      std::make_shared<SHI::MeasurementMetaData>("Temperature", "°C",
                                                 SHI::SensorDataType::FLOAT);
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
#ifdef HAS_DISPLAY
std::shared_ptr<SHI::SDS1306OLED> oled = std::make_shared<SHI::SDS1306OLED>(
    std::pair<std::string, String>(
        {"OutsideChannelDummyHumidity", "Feuchtigkeit"}),
    std::pair<std::string, String>(
        {"OutsideChannelDummyTemperature", "Temperatur"}));
#endif

class PrintHierachyVisitor : public SHI::Visitor {
 public:
  void enterVisit(SHI::Sensor *sensor) override {
    result += std::string(indent, ' ') +
              "S:" + std::string(sensor->getQualifiedName()) + "\n";
    indent++;
  }
  void leaveVisit(SHI::Sensor *sensor) override { indent--; }
  void enterVisit(SHI::SensorGroup *channel) override {
    result += std::string(indent, ' ') +
              "CH:" + std::string(channel->getQualifiedName()) + "\n";
    indent++;
  }
  void leaveVisit(SHI::SensorGroup *channel) override { indent--; }
  void enterVisit(SHI::Hardware *harwdware) override {
    result += std::string(indent, ' ') +
              "HW:" + std::string(harwdware->getQualifiedName()) + "\n";
    indent++;
  }
  void leaveVisit(SHI::Hardware *harwdware) override { indent--; }
  void visit(SHI::Communicator *communicator) override {
    result += std::string(indent, ' ') +
              "C:" + std::string(communicator->getQualifiedName()) + "\n";
  }
  void visit(SHI::MeasurementMetaData *data) override {
    result += std::string(indent, ' ') + "MD:" + data->getQualifiedName() +
              " unit:" + data->unit +
              " type:" + DATA_TYPES[static_cast<int>(data->type)] + "\n";
  }

  int indent = 0;
  std::string result = "";

 private:
  const std::string DATA_TYPES[4] = {"INT", "FLOAT", "STRING", "STATUS"};
};

void setup() {
  ets_printf("Starting\n");
  StaticJsonDocument<100> doc;
  deserializeJson(doc, "{}");
  ets_printf("Parsed JSON\n");
  SHI::ESP32HWConfig config(doc.as<JsonObject>());
  ets_printf("Created config\n");
  SHI::hw = new SHI::ESP32HW(config);
  ets_printf("Constructed HW\n");
  SHI::hw->addCommunicator(mqtt);
  SHI::hw->addCommunicator(multicastComms);
#ifdef HAS_DISPLAY
  SHI::hw->addCommunicator(oled);
  oled->setBrightness(5);
#endif
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
  SHI::hw->logInfo("WifiBaseTest", __func__,
                   (String("Heap is ") + ESP.getFreeHeap()).c_str());
  SHI::hw->loop();
  dummy->humidtyValue += 1;
  dummy->temperatureValue += 2.3;
  delay(1000);
}
