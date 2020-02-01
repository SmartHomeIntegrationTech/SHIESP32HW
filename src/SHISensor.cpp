#include "SHISensor.h"
#include <Arduino.h>
namespace SHI {
String FLOAT_TOSTRING(SHI::SensorData &data) {
  return String(data.floatValue, 1);
}
String INT_TOSTRING(SHI::SensorData &data) { return String(data.intValue); };
String STRING_TOSTRING(SHI::SensorData &data) { return data.stringValue; };
} // namespace SHI
