/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>

#include <string>
#include <utility>
#include <vector>

#include "SHICommunicator.h"
#include "SHISensor.h"

namespace SHI {

class RestCommunicator : public Communicator {
 public:
  explicit RestCommunicator(String prefix = "s_ESP32_")
      : Communicator("OpenhabREST"), prefix(prefix) {}
  void setupCommunication() override {}
  void loopCommunication() override {}
  void newReading(const MeasurementBundle &reading) override;
  void newStatus(const SHIObject &sensor, const char *message,
                 bool isFatal) override;
  void newHardwareStatus(const char *message) override;
  std::vector<std::pair<std::string, std::string>> getStatistics() override;

 private:
  int errorCount = 0, httpErrorCount = 0, httpCount = 0;
  String prefix;
  void uploadInfo(const std::string &item, const std::string &value,
                  bool tryHard = false);
  void printError(HTTPClient *http, int httpCode);
};

}  // namespace SHI
