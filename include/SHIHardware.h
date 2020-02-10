#pragma once
#include <Arduino.h>
#include <SHIObject.h>

namespace SHI {

class Hardware : public SHI::SHIObject{
public:
  virtual void resetWithReason(const char *reason, bool restart) = 0;
  virtual void errLeds(void) = 0;

  virtual void setupWatchdog() = 0;
  virtual void feedWatchdog() = 0;
  virtual void disableWatchdog() = 0;
  
  virtual String getNodeName() = 0;
  virtual String getResetReason() = 0;
  virtual void resetConfig() = 0;
  virtual void printConfig() = 0;

  virtual void addSensor(std::shared_ptr<SHI::Sensor> sensor) = 0;
  virtual void
  addCommunicator(std::shared_ptr<SHI::Communicator> communicator) = 0;
  virtual void setup(String defaultName) = 0;
  virtual void loop() = 0;

  void logInfo(String name, const char *func, String message) {
    log("INFO: " + name + "." + func + "() " + message);
  };
  void logWarn(String name, const char *func, String message) {
    log("WARN: " + name + "." + func + "() " + message);
  };
  void logError(String name, const char *func, String message) {
    log("ERROR: " + name + "." + func + "() " + message);
  };

protected:
  Hardware(String name) : SHIObject(name) {}
  virtual void log(String message) = 0;
};

} // namespace SHI
