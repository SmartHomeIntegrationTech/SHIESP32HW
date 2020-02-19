/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <Arduino.h>
#include <SHIObject.h>

#include <memory>

namespace SHI {

extern const uint8_t MAJOR_VERSION;
extern const uint8_t MINOR_VERSION;
extern const uint8_t PATCH_VERSION;
extern const char *VERSION;

class Hardware : public SHI::SHIObject {
 public:
  virtual void resetWithReason(const char *reason, bool restart) = 0;
  virtual void errLeds(void) = 0;

  virtual void setupWatchdog() = 0;
  virtual void feedWatchdog() = 0;
  virtual void disableWatchdog() = 0;

  virtual const char *getNodeName() = 0;
  virtual const char *getResetReason() = 0;
  virtual void resetConfig() = 0;
  virtual void printConfig() = 0;

  virtual void addSensor(std::shared_ptr<SHI::Sensor> sensor) = 0;
  virtual void addCommunicator(
      std::shared_ptr<SHI::Communicator> communicator) = 0;
  virtual void setup(const char *defaultName) = 0;
  virtual void loop() = 0;

  void logInfo(String name, const char *func, String message) {
    log("INFO: " + name + "." + func + "() " + message);
  }
  void logWarn(String name, const char *func, String message) {
    log("WARN: " + name + "." + func + "() " + message);
  }
  void logError(String name, const char *func, String message) {
    log("ERROR: " + name + "." + func + "() " + message);
  }

 protected:
  explicit Hardware(const char *name) : SHIObject(name) {}
  virtual void log(String message) = 0;
};

extern SHI::Hardware *hw;

}  // namespace SHI
