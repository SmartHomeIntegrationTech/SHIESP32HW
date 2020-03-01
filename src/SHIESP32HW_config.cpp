/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

// WARNING, this is an automatically generated file!
// Don't change anything in here.

#include "SHIESP32HW_config.h"

#include <iostream>
#include <string>
namespace {
const size_t capacity = JSON_OBJECT_SIZE(22);
}
SHI::ESP32HWConfig::ESP32HWConfig(JsonObject obj)
    : ssid(obj["ssid"] | "Elfenburg"),
      password(obj["password"] | "fe-shnyed-olv-ek"),
      local_IP(obj["local_IP"]),
      subnet(obj["subnet"] | 0x00FFFFFF),
      gateway(obj["gateway"] | 0x01bca8c0),
      primaryDNS(obj["primaryDNS"] | 0xfabca8c0),
      secondaryDNS(obj["secondaryDNS"] | 0xfabca8c0),
      ntp(obj["ntp"] | 0x01bca8c0),
      name(obj["name"] | "Testing"),
      baseURL(obj["baseURL"] | "http://192.168.188.250/esp/"),
      reconnectDelay(obj["reconnectDelay"] | 500),
      reconnectAttempts(obj["reconnectAttempts"] | 10),
      wdtTimeout(obj["wdtTimeout"] | 15000),
      CONNECT_TIMEOUT(obj["CONNECT_TIMEOUT"] | 500),
      DATA_TIMEOUT(obj["DATA_TIMEOUT"] | 1000),
      ntpServer(obj["ntpServer"] | "192.168.188.1"),
      gmtOffset_sec(obj["gmtOffset_sec"] | 3600),
      daylightOffset_sec(obj["daylightOffset_sec"] | 3600),
      ERR_LED(obj["ERR_LED"] | BUILTIN_LED),
      baudRate(obj["baudRate"] | 115200),
      disableUART(obj["disableUART"] | false),
      debugLevel(obj["debugLevel"] | 0) {}

void SHI::ESP32HWConfig::fillData(JsonDocument &doc) {
  doc["ssid"] = ssid;
  doc["password"] = password;
  doc["local_IP"] = local_IP;
  doc["subnet"] = subnet;
  doc["gateway"] = gateway;
  doc["primaryDNS"] = primaryDNS;
  doc["secondaryDNS"] = secondaryDNS;
  doc["ntp"] = ntp;
  doc["name"] = name;
  doc["baseURL"] = baseURL;
  doc["reconnectDelay"] = reconnectDelay;
  doc["reconnectAttempts"] = reconnectAttempts;
  doc["wdtTimeout"] = wdtTimeout;
  doc["CONNECT_TIMEOUT"] = CONNECT_TIMEOUT;
  doc["DATA_TIMEOUT"] = DATA_TIMEOUT;
  doc["ntpServer"] = ntpServer;
  doc["gmtOffset_sec"] = gmtOffset_sec;
  doc["daylightOffset_sec"] = daylightOffset_sec;
  doc["ERR_LED"] = ERR_LED;
  doc["baudRate"] = baudRate;
  doc["disableUART"] = disableUART;
  doc["debugLevel"] = debugLevel;
}
std::string SHI::ESP32HWConfig::toJson() {
  DynamicJsonDocument doc(capacity);
  fillData(doc);
  char output[2000];
  serializeJson(doc, output, sizeof(output));
  return std::string(output);
}
void SHI::ESP32HWConfig::printJson(std::ostream printer) {
  DynamicJsonDocument doc(capacity);
  fillData(doc);
  serializeJson(doc, printer);
}
