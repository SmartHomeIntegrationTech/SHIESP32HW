#pragma once
#include <Arduino.h>

namespace SHI {

class SHIHardware {
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

  virtual void addChannel(std::shared_ptr<SHI::Channel> channel) = 0;
  virtual void
  addCommunicator(std::shared_ptr<SHI::SHICommunicator> communicator) = 0;
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
  SHIHardware(String name) : name(name) {}
  virtual void log(String message) = 0;
  String name;
};

} // namespace SHI
