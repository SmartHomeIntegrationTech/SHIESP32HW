/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIMQTT.h"

#include <Arduino.h>

#include "LeifHomieLib.h"
#include "SHICommunicator.h"
#include "SHIHardware.h"
#include "SHIVisitor.h"

namespace {

class HomieHierachyVisitor : public SHI::Visitor {
 public:
  explicit HomieHierachyVisitor(HomieDevice *homie) : homie(homie) {}
  void enterVisit(SHI::Sensor *sensor) override {
    pNode = homie->NewNode();
    auto nodeName =
        String(isInChannel ? channelName.c_str() : sensor->getName());
    pNode->strFriendlyName = nodeName;
    nodeName.toLowerCase();
    pNode->strID = nodeName;
  }
  void leaveVisit(SHI::Sensor *sensor) override {}
  void enterVisit(SHI::SensorGroup *channel) override {
    isInChannel = true;
    channelName = std::string(channel->getName());
  }
  void leaveVisit(SHI::SensorGroup *channel) override {
    isInChannel = false;
    channelName = nullptr;
  }
  void enterVisit(SHI::Hardware *harwdware) override {}
  void leaveVisit(SHI::Hardware *harwdware) override {}
  void visit(SHI::Communicator *communicator) override {}
  void visit(SHI::MeasurementMetaData *data) override {
    auto prop = pNode->NewProperty();
    auto name = String(data->name);
    prop->strFriendlyName = name;
    name.toLowerCase();
    prop->strID = name;
    prop->strUnit = String(data->unit);
    switch (data->type) {
      case SHI::SensorDataType::FLOAT:
        prop->datatype = homieFloat;
        break;
      case SHI::SensorDataType::INT:
        prop->datatype = homieInt;
        break;
      case SHI::SensorDataType::STRING:
        prop->datatype = homieString;
        break;
    }
  }

 private:
  HomieDevice *homie;
  HomieNode *pNode;
  bool isInChannel = false;
  std::string channelName;
};
}  // namespace

void SHI::MQTT::setupCommunication() {
  String nodeName = String(SHI::hw->getNodeName());
  homie.strFriendlyName = nodeName + " " + String(hw->getName());
  nodeName.toLowerCase();
  homie.strID = nodeName;

  homie.strMqttServerIP = "192.168.188.250";
  homie.strMqttUserName = "esphomie";
  homie.strMqttPassword = "Jtsvc9TsP5NGfek8";

  HomieHierachyVisitor visitor(&homie);
  homie.Init();
}

void SHI::MQTT::loopCommunication() {}
void SHI::MQTT::newReading(const SHI::MeasurementBundle &reading,
                           const SHI::Sensor &sensor) {}
void SHI::MQTT::newStatus(const SHI::Sensor &sensor, const char *message,
                          bool isFatal) {}
void SHI::MQTT::newHardwareStatus(const char *message) {}
