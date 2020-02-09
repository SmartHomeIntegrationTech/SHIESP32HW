#pragma once
#include "SHICommunicator.h"
#include "SHIHardware.h"
#include "SHISensor.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <Preferences.h>
#include <WiFi.h>
#include <memory>

namespace SHI {

class SHIPrinter : public Print {
public:
  virtual void begin(int baudRate) = 0;
  virtual size_t write(uint8_t data) = 0;
};

class HWBase : public SHI::SHIHardware {
public:
  HWBase() : SHIHardware("ESP32") {}
  String getNodeName() override;

  void setupWatchdog() override;
  void feedWatchdog() override;
  void disableWatchdog() override;

  void addSensor(std::shared_ptr<SHI::Sensor> sensor) override {
    sensors.push_back(sensor);
  }
  void addCommunicator(std::shared_ptr<SHI::SHICommunicator> communicator) override {
    communicators.push_back(communicator);
  }

  String getResetReason() override;
  void resetWithReason(const char *reason, bool restart) override;
  void errLeds(void) override;

  void setup(String altName) override;
  void loop() override;

  void printConfig() override;
  void resetConfig() override;

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

  void wifiDisconnected(WiFiEventInfo_t info);
  void wifiConnected();

  std::vector<std::shared_ptr<SHI::Sensor>> sensors;
  std::vector<std::shared_ptr<SHI::SHICommunicator>> communicators;
  SHIPrinter *debugSerial;
  Preferences configPrefs;
  config_t config;
  hw_timer_t *timer = NULL;
};

extern HWBase hw;
} // namespace SHI
