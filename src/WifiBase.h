#ifndef WIFIBASE_H
#include <Arduino.h>

typedef struct {
  uint32_t canary;
  IPAddress local_IP;
  IPAddress gateway;
  IPAddress subnet;
  char name[20];
  char resetReason[40];
} config_t;
extern config_t config;

void setupWatchdog();
void feedWatchdog();
void resetWithReason(String reason, bool restart);
void uploadInfo(String prefix, String item, String value);
void uploadInfo(String prefix, String item, float value);
void uploadInfo(String item, String value);
void uploadInfo(String item, float value);
bool wifiIsConntected();
void wifiDoSetup(String defaultName);
void errLeds(void);

#define WIFIBASE_H
#endif