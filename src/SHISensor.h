#pragma once
#include <Arduino.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
// SHI stands for SmartHomeIntegration
namespace SHI {

enum SensorDataType { INT, FLOAT, STRING, ERROR, NO_DATA };

struct SensorData {
  SensorData(SensorDataType type,
             std::function<String(SensorData &)> toTransmitString, String name)
      : type(type), toTransmitString(toTransmitString), name(name), intValue(0),
        stringValue("") {}
  SensorData(SensorData *data)
      : type(data->type), toTransmitString(data->toTransmitString),
        name(data->name), intValue(0), stringValue("") {}
  SensorDataType type;
  std::function<String(SensorData &)> toTransmitString;
  String name;
  union {
    float floatValue;
    int intValue;
  };
  String stringValue;
};

struct SensorReadings {
  SensorReadings(unsigned long timeStamp,
                 std::vector<std::shared_ptr<SHI::SensorData>> data)
      : timeStamp(timeStamp), data(data) {}
  SensorReadings(SensorReadings *readings)
      : timeStamp(readings->timeStamp), data(readings->data) {}
  unsigned long timeStamp;
  std::vector<std::shared_ptr<SHI::SensorData>> data;
};

class Sensor {
public:
  virtual std::shared_ptr<SensorReadings> readSensor() = 0;
  virtual bool setupSensor() = 0;
  virtual String getStatusMessage() = 0;
  virtual bool errorIsFatal() = 0;
  virtual String getName() = 0;
};

String FLOAT_TOSTRING(SHI::SensorData &data);
String INT_TOSTRING(SHI::SensorData &data);
String STRING_TOSTRING(SHI::SensorData &data);

} // namespace SHI
