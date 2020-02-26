/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once

#include <map>
#include <string>

#include "SHICommunicator.h"
#include "SHIObject.h"
#include "SHISensor.h"
#include "ext/LeifHomieLib.h"

namespace SHI {

class MQTT : public Communicator {
 public:
  MQTT() : Communicator("MQTT") {}
  void setupCommunication() override;
  void loopCommunication() override;
  void newReading(const MeasurementBundle &reading) override;
  void newStatus(const SHIObject &sensor, const char *message,
                 bool isFatal) override;
  void newHardwareStatus(const char *message) override;

 private:
  HomieDevice homie;
  std::map<std::string, HomieProperty *> nameToProps;
};

}  // namespace SHI
