/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIMQTT.h"

#include <Arduino.h>
#include <time.h>

#include "SHICommunicator.h"
#include "SHIHardware.h"
#include "SHIVisitor.h"
#include "ext/LeifHomieLib.h"

namespace {

class HomieHierachyVisitor : public SHI::Visitor {
 public:
  explicit HomieHierachyVisitor(
      HomieDevice *homie, std::map<std::string, HomieProperty *> *nameToProp)
      : homie(homie), nameToProp(nameToProp) {}
  void enterVisit(SHI::Sensor *sensor) override {
    SHI_LOGINFO("Visiting sensor");
    pNode = homie->NewNode();
    auto qfn = sensor->getQualifiedName("-");
    auto nodeName = String(qfn.c_str());
    pNode->strFriendlyName = nodeName;
    nodeName.toLowerCase();
    pNode->strID = nodeName;
  }
  void leaveVisit(SHI::Sensor *sensor) override {}
  void enterVisit(SHI::SensorGroup *channel) override {}
  void leaveVisit(SHI::SensorGroup *channel) override {}
  void enterVisit(SHI::Hardware *hardware) override {}
  void leaveVisit(SHI::Hardware *hardware) override {}
  void visit(SHI::Communicator *communicator) override {}
  void visit(SHI::MeasurementMetaData *data) override {
    SHI_LOGINFO("Visiting metaData");
    auto prop = pNode->NewProperty();
    auto qfn = data->getQualifiedName();
    auto nameString = String(data->getName());
    prop->strFriendlyName = nameString;
    nameToProp->insert({qfn, prop});
    SHI_LOGINFO("Registered:" + qfn);
    nameString.toLowerCase();
    prop->strID = nameString;
    prop->strUnit = String(data->unit);
    switch (data->type) {
      case SHI::SensorDataType::FLOAT:
        prop->datatype = homieFloat;
        break;
      case SHI::SensorDataType::INT:
        prop->datatype = homieInt;
        break;
      case SHI::SensorDataType::STATUS:
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
  const char *name = "HomieVisitor";
  std::map<std::string, HomieProperty *> *nameToProp;
};
}  // namespace

void SHI::MQTT::setupCommunication() {
  SHI_LOGINFO("Setting up");
  String nodeName = String(SHI::hw->getNodeName());
  homie.strFriendlyName = nodeName + " " + String(hw->getName());
  nodeName.toLowerCase();
  homie.strID = nodeName;

  homie.strMqttServerIP = "192.168.188.250";
  homie.strMqttUserName = "esphomie";
  homie.strMqttPassword = "Jtsvc9TsP5NGfek8";

  HomieHierachyVisitor visitor(&homie, &nameToProps);
  SHI::hw->accept(visitor);
  SHI_LOGINFO("Visitor succeeded");
  homie.Init();
}

void SHI::MQTT::loopCommunication() {
  SHI_LOGINFO(std::string("Connection status:") +
              (homie.IsConnected() ? "Connected" : "Disconnected"));
  homie.Loop();
}
void SHI::MQTT::newReading(const SHI::MeasurementBundle &reading) {
  auto influxFormat = String(reading.src->getName()) +
                      ",qfn=" + reading.src->getQualifiedName().c_str() + " ";
  bool first = true;
  for (auto &&data : reading.data) {
    if (data.getDataState() != SHI::MeasurementDataState::VALID) continue;
    auto qfn = data.getMetaData()->getQualifiedName();
    auto prop = nameToProps.find(qfn);
    auto value = String(data.toTransmitString().c_str());
    if (!first) influxFormat += ',';
    first = false;
    auto simpleName = String(data.getMetaData()->getName());
    influxFormat += simpleName + '=' + value;
    if (prop != nameToProps.end()) {
      prop->second->SetValue(value);
    } else {
      SHI_LOGERROR("Did not find:" + qfn);
    }
  }
  int msPart = reading.timeStamp % 1000;  // 10 digits
  int sPart = reading.timeStamp / 1000;   // 3 digits
  char buf[10 + 3 + 6 + 2];  // 6 digits for ns plus 0 and whitespace
  snprintf(buf, sizeof(buf), " %d%03d000000", sPart, msPart);
  influxFormat += buf;
  String topic = String("sensors/") + reading.src->getQualifiedName().c_str() +
                 "/influxformat";
  String payload = influxFormat;
  // SHI_LOGINFO(std::string("Influx representation: ") + topic.c_str() + " " +
  //            payload.c_str());
  homie.PublishDirect(topic, 1, false, payload);
}
void SHI::MQTT::newStatus(const SHI::SHIObject &sensor, const char *message,
                          bool isFatal) {}
void SHI::MQTT::newHardwareStatus(const char *message) {}
