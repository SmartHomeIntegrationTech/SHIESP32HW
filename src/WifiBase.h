#pragma once
#include "SHISensor.h"
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

class HWBase {
public:
  String getConfigName();

  void setupWatchdog();
  void feedWatchdog();

  void addSensor(SHI::Sensor &sensor);

  void setDisplayBrightness(uint8_t value);

  std::unique_ptr<Print> debugSerial;

  void addUDPPacketHandler(String trigger, AuPacketHandlerFunction handler);

  void resetWithReason(String reason, bool restart);
  void errLeds(void);

  void setup(String altName);
  void loop();

private:
  void uploadInfo(String prefix, String item, String value);
  bool wifiIsConntected();
  void wifiDoSetup(String defaultName);
  class NullPrint : public Print {
  public:
    NullPrint() {}
    size_t write(uint8_t) { return 1; }
    void begin(int baudRate) {}
  };
};
extern const int CONNECT_TIMEOUT;
extern const int DATA_TIMEOUT;
extern const String STATUS_ITEM;
extern const String STATUS_OK;

extern const uint8_t MAJOR_VERSION;
extern const uint8_t MINOR_VERSION;
extern const uint8_t PATCH_VERSION;
extern const String VERSION;

extern HWBase hw;
} // namespace SHI
