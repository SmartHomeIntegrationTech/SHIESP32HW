/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include <Arduino.h>
#include <SHISensor.h>
namespace SHI {

const char* FLOAT_TOSTRING(const SHI::SensorData& data) {
  char* buf = new char[33];
  return dtostrf(data.floatValue, (1 + 2), 1, buf);
}
const char* INT_TOSTRING(const SHI::SensorData& data) {
  char* buf = new char[2 + 8 * sizeof(int)];
  snprintf(buf, 2 + 8 * sizeof(int), "%d", data.intValue);
  return buf;
}
const char* STRING_TOSTRING(const SHI::SensorData& data) {
  return data.stringValue;
}

const char* STATUS_ITEM = "Status";
const char* STATUS_OK = "OK";

}  // namespace SHI

void SHI::Channel::accept(SHI::Visitor& visitor) {
  visitor.visit(this);
  sensor->accept(visitor);
}
