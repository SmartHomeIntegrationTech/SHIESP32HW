/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <Arduino.h>
#include <AsyncUDP.h>
#include <Preferences.h>
#include <WiFi.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "SHICommunicator.h"
#include "SHIHardware.h"
#include "SHISensor.h"

namespace SHI {

class SHIPrinter : public Print {
 public:
  virtual void begin(int baudRate) = 0;
  virtual size_t write(uint8_t data) = 0;
};

class ESP32HW : public Hardware {
 public:
  ESP32HW() : Hardware("ESP32") {}
  std::string getNodeName() override;
  const std::string getName() const override;

  void setupWatchdog() override;
  void feedWatchdog() override;
  void disableWatchdog() override;

  int64_t getEpochInMs() override;

  std::string getResetReason() override;
  void resetWithReason(const std::string &reason, bool restart) override;
  void errLeds(void) override;

  void setup(const std::string &altName) override;
  void loop() override;

  void printConfig() override;
  void resetConfig() override;

  std::vector<std::pair<std::string, std::string>> getStatistics() override;

  void logInfo(const std::string &name, const char *func,
               const String &message) {
    log("INFO: " + String(name.c_str()) + "." + String(func) + "() " + message);
  }
  void logWarn(const std::string &name, const char *func,
               const String &message) {
    log("INFO: " + String(name.c_str()) + "." + String(func) + "() " + message);
  }
  void logError(const std::string &name, const char *func,
                const String &message) {
    log("INFO: " + String(name.c_str()) + "." + String(func) + "() " + message);
  }

 protected:
  void log(const String &message);
  void log(const std::string &message) override;

 private:
  struct config_t {
    uint32_t canary;
    uint32_t local_IP;
    uint32_t gateway;
    uint32_t subnet;
    char name[20];
    char resetReason[40];
  };
  bool wifiIsConntected();
  void wifiDoSetup(String defaultName);
  bool updateNodeName();
  void setupWifiFromConfig(const std::string &defaultName);
  void initialWifiConnect();
  void storeWifiConfig();

  void wifiDisconnected(WiFiEventInfo_t info);
  void wifiConnected();

  SHIPrinter *debugSerial;
  Preferences configPrefs;
  config_t config;
  hw_timer_t *timer = NULL;
  int connectCount = 0, retryCount = 0;
  uint32_t sensorSetupTime = 0, initialWifiConnectTime = 0, commSetupTime = 0;
  float averageSensorLoopDuration = 0, averageConnectDuration = 0;
  std::string internalStatus = SHI::STATUS_OK;
};

}  // namespace SHI
