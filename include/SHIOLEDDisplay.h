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
  SHIOLEDDisplay(String firstRow="", String secondRow="", String thirdRow="")
      : SHICommunicator("OLEDDisplay") {
/*    displayItems.insert({firstRow, 0});
    displayItems.insert({secondRow, 1});
    displayItems.insert({thirdRow, 2});*/
  };
  void setupCommunication() override;
  void loopCommunication() override;
  void newReading(SHI::SensorReadings &reading, SHI::Channel &channel) override;
  void newStatus(SHI::Channel &channel, String message, bool isFatal) override;
  void newHardwareStatus(String message) override;
  void setBrightness(uint8_t level);
private:
  //std::unordered_map<String, int> displayItems;
};

} // namespace SHI
