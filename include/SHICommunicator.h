/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include "SHIObject.h"
#include "SHISensor.h"

namespace SHI {

class Communicator : public SHI::SHIObject {
 public:
  virtual void wifiConnected() { isConnected = true; }
  virtual void wifiDisconnected() { isConnected = false; }
  virtual void setupCommunication() = 0;
  virtual void loopCommunication() = 0;
  virtual void newReading(SHI::SensorReadings &reading, SHI::Sensor &sensor) {}
  virtual void newStatus(SHI::Sensor &sensor, String message, bool isFatal) {}
  virtual void newHardwareStatus(String message) {}

 protected:
  explicit Communicator(String name) : SHIObject(name) {}
  bool isConnected = false;
};

}  // namespace SHI
