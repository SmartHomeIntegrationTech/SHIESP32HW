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

struct config_t {
  uint32_t canary;
  uint32_t local_IP;
  uint32_t gateway;
  uint32_t subnet;
  char name[20];
  char resetReason[40];
};
extern config_t config;

class SHIPrinter : public Print {
public:
  virtual void begin(int baudRate) = 0;
  virtual size_t write(uint8_t data) = 0;
};

class HWBase : public SHI::SHIHardware {
public:
  HWBase() : SHIHardware("ESP32") {}
  String getNodeName();

  void setupWatchdog();
  void feedWatchdog();
  void disableWatchdog();

  void setDisplayBrightness(uint8_t value);

  void addChannel(std::shared_ptr<SHI::Channel> channel) {
    channels.push_back(channel);
  }
  void addCommunicator(std::shared_ptr<SHI::SHICommunicator> communicator) {
    communicators.push_back(communicator);
  }

  void addUDPPacketHandler(String trigger, AuPacketHandlerFunction handler);

  void resetWithReason(const char *reason, bool restart);
  void errLeds(void);

  void setup(String altName);
  void loop();
  void printConfig();
  void resetConfig();

protected:
  void log(String message);

private:
  void uploadInfo(String prefix, String item, String value);
  bool wifiIsConntected();
  void wifiDoSetup(String defaultName);
  bool updateNodeName();
  void wifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
  void wifiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
  std::vector<std::shared_ptr<SHI::Channel>> channels;
  std::vector<std::shared_ptr<SHI::SHICommunicator>> communicators;
  SHIPrinter *debugSerial;
  Preferences configPrefs;
};

extern HWBase hw;
} // namespace SHI
