#pragma once
#include "SHICommunicator.h"
#include <unordered_map>
#include <vector>

namespace std {
template <> struct hash<String> {
  std::size_t operator()(String const &s) const noexcept {
    return std::hash<std::string>{}(s.c_str());
  }
};
} // namespace std

namespace SHI {

class SHIOLEDDisplay : public SHI::SHICommunicator {
public:
  SHIOLEDDisplay(std::pair<String, String> firstRow = {"", ""},
                 std::pair<String, String> secondRow = {"", ""},
                 std::pair<String, String> thirdRow = {"", ""})
      : SHICommunicator("OLEDDisplay") {
    displayItems.insert({firstRow.first, 0});
    displayItems.insert({secondRow.first, 1});
    displayItems.insert({thirdRow.first, 2});
    displayLineBuf[0]=firstRow.second;
    displayLineBuf[2]=secondRow.second;
    displayLineBuf[4]=thirdRow.second;
  };
  void setupCommunication() override;
  void loopCommunication() override;
  void newReading(SHI::SensorReadings &reading, SHI::Sensor &sensor) override;
  void newStatus(SHI::Sensor &sensor, String message, bool isFatal) override;
  void newHardwareStatus(String message) override;
  void setBrightness(uint8_t level);

private:
  std::unordered_map<String, int> displayItems;
  String displayLineBuf[7] = {"", "", "", "", "", "", ""};
};

} // namespace SHI
