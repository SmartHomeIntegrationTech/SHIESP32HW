/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#pragma once

#include <iostream>
#include <string>

#include "ArduinoJson.h"

#ifndef BUILTIN_LED
#warning "Did not define BUILTIN_LED"
#define BUILTIN_LED -1
#endif
namespace SHI {
class ESP32HWConfig {
 public:
  explicit ESP32HWConfig(JsonObject obj);
  std::string toJson();
  void printJson(std::ostream printer);
  void fillData(JsonDocument &doc);  // NOLINT Yes, non constant reference
  const std::string ssid = "Elfenburg";
  const std::string password = "fe-shnyed-olv-ek";

  uint32_t local_IP;
  uint32_t subnet = 0x00FFFFFF;
  const uint32_t gateway = 0x01bca8c0;  // 0xC0A8BC01;
  const uint32_t primaryDNS = 0xfabca8c0;
  const uint32_t secondaryDNS = 0xfabca8c0;
  const uint32_t ntp = 0x01bca8c0;
  const std::string name = "Testing";
  const std::string baseURL = "http://192.168.188.250/esp/";

  const int reconnectDelay = 500;
  const int reconnectAttempts = 10;

  const int wdtTimeout = 15000;  // time in ms to trigger the watchdog
  const int CONNECT_TIMEOUT = 500;
  const int DATA_TIMEOUT = 1000;

  const std::string ntpServer = "192.168.188.1";
  const int gmtOffset_sec = 3600;
  const int daylightOffset_sec = 3600;
  const int ERR_LED = BUILTIN_LED;

  const int baudRate = 115200;
  const bool disableUART = false;
  const int debugLevel = 0;
};
}  // namespace SHI
