#pragma once
#include "SHICommunicator.h"
#include "SHISensor.h"
#include <HTTPClient.h>
#include <map>

namespace SHI {

class SHIRestCommunicator : public SHI::SHICommunicator {
public:
  SHIRestCommunicator() : SHICommunicator("OpenhabREST") {}
  void wifiConnected() { isCurrentlyConnected = true; };
  void wifiDisconnected() { isCurrentlyConnected = false; };
  void setupCommunication(){};
  void loopCommunication(){};
  void newReading(SHI::SensorReadings &reading, SHI::Channel &channel);
  void newStatus(SHI::Channel &channel, String message, bool isFatal);
  void newHardwareStatus(String message);

protected:
  bool isCurrentlyConnected=false;
  int errorCount = 0, httpErrorCount = 0, httpCount = 0;

private:
  void uploadInfo(String name, String item, String value);
  void printError(HTTPClient &http, int httpCode);
};

} // namespace SHI