#ifndef _SHISensor_h
#include <Arduino.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// SHI stands for SmartHomeIntegration
namespace SHI {

extern const String STATUS_ITEM;
extern const String STATUS_OK;

extern const uint8_t MAJOR_VERSION;
extern const uint8_t MINOR_VERSION;
extern const uint8_t PATCH_VERSION;
extern const String VERSION;

enum SensorDataType { INT, FLOAT, STRING, NO_DATA };

struct SensorData {
  SensorData(SensorDataType type,
             std::function<String(SensorData &)> toTransmitString, String name)
      : type(type), toTransmitString(toTransmitString), name(name),
        intValue(0) {}
  SensorData(SensorData *data)
      : type(data->type), toTransmitString(data->toTransmitString),
        name(data->name), intValue(data->intValue) {}
  SensorDataType type;
  std::function<String(SensorData &)> toTransmitString;
  String name;
  union {
    float floatValue;
    int intValue;
    char *stringValue;
  };
};

struct SensorReadings {
  SensorReadings(std::vector<std::shared_ptr<SHI::SensorData>> data)
      : timeStamp(0), data(data) {}
  SensorReadings(SensorReadings *readings)
      : timeStamp(readings->timeStamp),
        data(readings->data) {}
  unsigned long timeStamp = 0;
  std::vector<std::shared_ptr<SHI::SensorData>> data = {};
};

class Sensor {
public:
  virtual std::shared_ptr<SensorReadings> readSensor() = 0;
  virtual bool setupSensor() = 0;
  virtual String getStatusMessage() { return statusMessage; };
  virtual bool errorIsFatal() { return fatalError; };
  virtual String getName() { return name; };

protected:
  Sensor(String name) : name(name){};
  String statusMessage = SHI::STATUS_OK;
  bool fatalError = false;
  String name;
};

class Channel {
public:
  Channel(std::shared_ptr<Sensor> sensor, String name = "")
      : sensor(sensor), name(name){};
  std::shared_ptr<Sensor> sensor;
  const String name;
};

String FLOAT_TOSTRING(SHI::SensorData &data);
String INT_TOSTRING(SHI::SensorData &data);
String STRING_TOSTRING(SHI::SensorData &data);

} // namespace SHI

#define _SHISensor_h
#endif
