/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include <Arduino.h>
#include <SHISensor.h>
namespace SHI {

String FLOAT_TOSTRING(const SHI::SensorData &data) {
  return String(data.floatValue, 1);
}
String INT_TOSTRING(const SHI::SensorData &data) {
  return String(data.intValue);
}
String STRING_TOSTRING(const SHI::SensorData &data) {
  return String(data.stringValue);
}

const String STATUS_ITEM = "Status";
const String STATUS_OK = "OK";

}  // namespace SHI

void SHI::Channel::accept(SHI::Visitor &visitor) {
  visitor.visit(this);
  sensor->accept(visitor);
}
