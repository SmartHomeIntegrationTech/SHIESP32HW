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

class ESP32HW : public SHI::Hardware {
 public:
  ESP32HW() : Hardware("ESP32") {}
  String getNodeName() override;

  void setupWatchdog() override;
  void feedWatchdog() override;
  void disableWatchdog() override;

  void addSensor(std::shared_ptr<SHI::Sensor> sensor) override {
    sensors.push_back(sensor);
  }
  void addCommunicator(
      std::shared_ptr<SHI::Communicator> communicator) override {
    communicators.push_back(communicator);
  }

  String getResetReason() override;
  void resetWithReason(const char *reason, bool restart) override;
  void errLeds(void) override;

  void setup(String altName) override;
  void loop() override;

  void printConfig() override;
  void resetConfig() override;

  void accept(SHI::Visitor &visitor) override;

  std::vector<std::pair<String, String>> getStatistics() override;

 protected:
  void log(String message);

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
  void setupSensors();
  void setupWifiFromConfig(String defaultName);
  void initialWifiConnect();
  void storeWifiConfig();

  void wifiDisconnected(WiFiEventInfo_t info);
  void wifiConnected();

  std::vector<std::shared_ptr<SHI::Sensor>> sensors;
  std::vector<std::shared_ptr<SHI::Communicator>> communicators;
  SHIPrinter *debugSerial;
  Preferences configPrefs;
  config_t config;
  hw_timer_t *timer = NULL;
  int connectCount = 0, retryCount = 0;
  uint32_t sensorSetupTime = 0, initialWifiConnectTime = 0, commSetupTime = 0;
  float averageSensorLoopDuration = 0, averageConnectDuration = 0;
};

}  // namespace SHI
