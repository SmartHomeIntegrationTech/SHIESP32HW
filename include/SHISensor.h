/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <Arduino.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "SHIObject.h"

// SHI stands for SmartHomeIntegration
namespace SHI {

extern const String STATUS_ITEM;
extern const String STATUS_OK;

extern const uint8_t MAJOR_VERSION;
extern const uint8_t MINOR_VERSION;
extern const uint8_t PATCH_VERSION;
extern const String VERSION;

enum class SensorDataType { INT, FLOAT, STRING, NO_DATA };

struct SensorData {
  SensorData(SensorDataType type,
             std::function<String(const SensorData &)> toTransmitString,
             String name, String unit = "")
      : type(type),
        toTransmitString(toTransmitString),
        name(name),
        intValue(0) {}
  SensorData(const SensorData &data)
      : type(data.type),
        toTransmitString(data.toTransmitString),
        name(data.name),
        unit(data.unit),
        intValue(data.intValue) {}
  SensorDataType type;
  std::function<String(const SensorData &)> toTransmitString;
  const String name;
  const String unit;
  union {
    float floatValue;
    int intValue;
    char *stringValue;
  };
};

struct SensorReadings {
  explicit SensorReadings(std::vector<std::shared_ptr<SHI::SensorData>> data)
      : timeStamp(0), data(data) {}
  SensorReadings(const SensorReadings &readings)
      : timeStamp(readings.timeStamp), data(readings.data) {}
  uint32_t timeStamp = 0;
  std::vector<std::shared_ptr<SHI::SensorData>> data = {};
};

class Sensor : public SHI::SHIObject {
 public:
  virtual std::shared_ptr<SensorReadings> readSensor() = 0;
  virtual bool setupSensor() = 0;
  virtual bool stopSensor() = 0;
  virtual String getStatusMessage() const { return statusMessage; }
  virtual bool errorIsFatal() const { return fatalError; }

 protected:
  explicit Sensor(const char *name) : SHIObject(name) {}
  String statusMessage = SHI::STATUS_OK;
  bool fatalError = false;
};

class Channel : public SHI::Sensor {
 public:
  Channel(std::shared_ptr<Sensor> sensor, const char *name)
      : Sensor(name), sensor(sensor) {}
  std::shared_ptr<SensorReadings> readSensor() override {
    return sensor->readSensor();
  };
  bool setupSensor() override { return sensor->setupSensor(); }
  bool stopSensor() override { return sensor->stopSensor(); }
  void accept(SHI::Visitor &visitor) override;
  String getStatusMessage() const override {
    return sensor->getStatusMessage();
  }
  bool errorIsFatal() const override { return sensor->errorIsFatal(); }
  const char *getName() const override {
    std::string total(std::string(name) + sensor->getName());
    return total.c_str();
  }
  const std::shared_ptr<Sensor> sensor;
};

String FLOAT_TOSTRING(const SHI::SensorData &data);
String INT_TOSTRING(const SHI::SensorData &data);
String STRING_TOSTRING(const SHI::SensorData &data);

}  // namespace SHI
