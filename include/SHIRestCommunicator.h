/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>

#include <utility>
#include <vector>

#include "SHICommunicator.h"
#include "SHISensor.h"

namespace SHI {

class RestCommunicator : public SHI::Communicator {
 public:
  RestCommunicator() : Communicator("OpenhabREST") {}
  void setupCommunication() override {}
  void loopCommunication() override {}
  void newReading(const SHI::SensorReadings &reading,
                  const SHI::Sensor &sensor) override;
  void newStatus(const SHI::Sensor &sensor, const char *message,
                 bool isFatal) override;
  void newHardwareStatus(const char *message) override;
  void accept(SHI::Visitor &visitor) override { visitor.visit(this); }
  std::vector<std::pair<const char *, const char *>> getStatistics() override;

 protected:
  int errorCount = 0, httpErrorCount = 0, httpCount = 0;

 private:
  void uploadInfo(String name, String item, String value);
  void printError(HTTPClient *http, int httpCode);
};

}  // namespace SHI
