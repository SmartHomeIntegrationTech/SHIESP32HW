#pragma once
#include "SHICommunicator.h"
#include "SHISensor.h"
#include <HTTPClient.h>
#include <map>

namespace SHI {

class RestCommunicator : public SHI::Communicator {
public:
  RestCommunicator() : Communicator("OpenhabREST") {}
  void setupCommunication() override {};
  void loopCommunication()  override {};
  void newReading(SHI::SensorReadings &reading, SHI::Sensor &sensor) override;
  void newStatus(SHI::Sensor &sensor, String message, bool isFatal) override;
  void newHardwareStatus(String message) override;
  void accept(SHI::Visitor &visitor) override {visitor.visit(this);};

protected:
  int errorCount = 0, httpErrorCount = 0, httpCount = 0;

private:
  void uploadInfo(String name, String item, String value);
  void printError(HTTPClient &http, int httpCode);
};

} // namespace SHI