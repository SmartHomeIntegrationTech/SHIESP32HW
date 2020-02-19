/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SHICommunicator.h"

namespace std {
template <>
struct hash<String> {
  std::size_t operator()(String const &s) const noexcept {
    return std::hash<std::string>{}(s.c_str());
  }
};
}  // namespace std

namespace SHI {

class OLEDDisplay : public SHI::Communicator {
 public:
  OLEDDisplay(std::pair<String, String> firstRow = {"", ""},
              std::pair<String, String> secondRow = {"", ""},
              std::pair<String, String> thirdRow = {"", ""})
      : Communicator("OLEDDisplay") {
    displayItems.insert({firstRow.first, 0});
    displayItems.insert({secondRow.first, 1});
    displayItems.insert({thirdRow.first, 2});
    displayLineBuf[0] = firstRow.second;
    displayLineBuf[2] = secondRow.second;
    displayLineBuf[4] = thirdRow.second;
  }
  void setupCommunication() override;
  void loopCommunication() override;
  void newReading(const SHI::SensorReadings &reading,
                  const SHI::Sensor &sensor) override;
  void newStatus(const SHI::Sensor &sensor, const char *message,
                 bool isFatal) override;
  void newHardwareStatus(const char *message) override;
  void accept(SHI::Visitor &visitor) override { visitor.visit(this); }
  void setBrightness(uint8_t level);

 private:
  std::unordered_map<String, int> displayItems;
  String displayLineBuf[7] = {"", "", "", "", "", "", ""};
};

}  // namespace SHI
