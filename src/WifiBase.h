#ifndef WIFIBASE_H
#include <Arduino.h>
#include <AsyncUDP.h>

typedef struct {
  uint32_t canary;
  uint32_t local_IP;
  uint32_t gateway;
  uint32_t subnet;
  char name[20];
  char resetReason[40];
} config_t;
extern config_t config;

void setupWatchdog();
void feedWatchdog();
void addUDPPacketHandler(String trigger, AuPacketHandlerFunction handler);
void resetWithReason(String reason, bool restart);
void uploadInfo(String prefix, String item, String value);
void uploadInfo(String prefix, String item, float value);
void uploadInfo(String item, String value);
void uploadInfo(String item, float value);
bool wifiIsConntected();
void wifiDoSetup(String defaultName);
void errLeds(void);

extern const int CONNECT_TIMEOUT;
extern const int DATA_TIMEOUT;
extern const String STATUS_ITEM;

#define WIFIBASE_H
#endif