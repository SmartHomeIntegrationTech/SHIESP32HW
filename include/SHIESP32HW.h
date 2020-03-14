/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncUDP.h>
#include <Preferences.h>
#include <WiFi.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "SHICommunicator.h"
#include "SHIFactory.h"
#include "SHIHardware.h"
#include "SHISensor.h"

namespace SHI {

#include "ArduinoJson.h"

#ifndef BUILTIN_LED
#warning "Did not define BUILTIN_LED"
#define BUILTIN_LED -1
#endif

class ESP32HWConfig : public Configuration {
 public:
  ESP32HWConfig() {}
  explicit ESP32HWConfig(const JsonObject &obj);
  int getExpectedCapacity() const override;
  void fillData(JsonObject &doc) const override;
  std::string ssid = "Elfenburg";
  std::string password = "fe-shnyed-olv-ek";

  std::string local_IP = "";
  std::string subnet = "255.255.255.0";
  std::string gateway = "192.168.188.1";  // 0xC0A8BC01;
  std::string primaryDNS = "192.168.188.1";
  std::string secondaryDNS = "192.168.188.1";
  std::string ntp = "192.168.188.1";
  std::string name = "Testing";
  std::string baseURL = "http://192.168.188.250/esp/";

  int reconnectDelay = 500;
  int reconnectAttempts = 10;

  int wdtTimeout = 15000;  // time in ms to trigger the watchdog
  int CONNECT_TIMEOUT = 500;
  int DATA_TIMEOUT = 1000;

  std::string ntpServer = "192.168.188.1";
  int gmtOffset_sec = 3600;
  int daylightOffset_sec = 3600;
  int ERR_LED = BUILTIN_LED;

  int baudRate = 115200;
  bool noSerialLogging = false;
  int debugLevel = 0;
};

class SHIPrinter : public Print {
 public:
  virtual void begin(int baudRate) = 0;
  virtual size_t write(uint8_t data) = 0;
};

class HardwareSHIPrinter : public SHI::SHIPrinter {
  void begin(int baudRate) { Serial.begin(baudRate); }
  size_t write(uint8_t data) { return Serial.write(data); }
};

class NullSHIPrinter : public SHI::SHIPrinter {
 public:
  size_t write(uint8_t) { return 1; }
  void begin(int baudRate) {}
};

class ESP32HW : public Hardware {
 public:
  explicit ESP32HW(const ESP32HWConfig &config)
      : Hardware("ESP32"), hwConfig(config) {
    if (config.noSerialLogging) {
      debugSerial = new NullSHIPrinter();
    } else {
      debugSerial = new HardwareSHIPrinter();
      debugSerial->begin(config.baudRate);
    }
  }
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

  const Configuration *getConfig() const override { return &hwConfig; }
  bool reconfigure(Configuration *newConfig) override {
    hwConfig = castConfig<ESP32HWConfig>(newConfig);
    return true;
  }
  void logInfo(const std::string &name, const char *func,
               const String &message) {
    if (hwConfig.debugLevel <= 0)
      log(String("INFO: ") + name.c_str() + "." + func + "() " + message);
  }
  void logWarn(const std::string &name, const char *func,
               const String &message) {
    if (hwConfig.debugLevel <= 1)
      log(String("WARN: ") + name.c_str() + "." + func + "() " + message);
  }
  void logError(const std::string &name, const char *func,
                const String &message) {
    if (hwConfig.debugLevel <= 2)
      log(String("ERROR: ") + name.c_str() + "." + func + "() " + message);
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
  ESP32HWConfig hwConfig;
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
