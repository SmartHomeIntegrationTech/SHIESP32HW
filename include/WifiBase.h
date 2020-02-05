#pragma once
#include "SHISensor.h"
#include "SHIHardware.h"
#include "SHICommunicator.h"
#include <Arduino.h>
#include <AsyncUDP.h>
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

class SHIPrinter: public Print {
  public:
    virtual void begin(int baudRate)=0;
    virtual size_t write(uint8_t data)=0;
};

class HWBase : public SHI::SHIHardware {
public:

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

  void resetWithReason(String reason, bool restart);
  void errLeds(void);

  void setup(String altName);
  void loop();
  void printConfig();

protected:
  void log(String message);

private:
  void uploadInfo(String prefix, String item, String value);
  bool wifiIsConntected();
  void wifiDoSetup(String defaultName);
  bool updateNodeName();
  std::vector<std::shared_ptr<SHI::Channel>> channels;
  std::vector<std::shared_ptr<SHI::SHICommunicator>> communicators;
  SHIPrinter *debugSerial;
};

extern const int CONNECT_TIMEOUT;
extern const int DATA_TIMEOUT;

extern HWBase hw;
} // namespace SHI
