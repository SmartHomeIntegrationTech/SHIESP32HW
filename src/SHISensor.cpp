#include "SHISensor.h"
#include <Arduino.h>
namespace SHI {

String FLOAT_TOSTRING(SHI::SensorData &data) {
  return String(data.floatValue, 1);
}
String INT_TOSTRING(SHI::SensorData &data) { return String(data.intValue); };
String STRING_TOSTRING(SHI::SensorData &data) {
  return String(data.stringValue);
};

#ifndef VER_MAJ
#error "Major version undefined"
#endif
#ifndef VER_MIN
#error "Minor version undefined"
#endif
#ifndef VER_PAT
#error "Patch version undefined"
#endif
const uint8_t MAJOR_VERSION = VER_MAJ;
const uint8_t MINOR_VERSION = VER_MIN;
const uint8_t PATCH_VERSION = VER_PAT;
const String VERSION = String(MAJOR_VERSION, 10) + "." +
                       String(MINOR_VERSION, 10) + "." +
                       String(PATCH_VERSION, 10);

const String STATUS_ITEM = "Status";
const String STATUS_OK = "OK";

} // namespace SHI
