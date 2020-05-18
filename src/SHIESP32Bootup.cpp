/**
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include <Arduino.h>
#include <HTTPClient.h>
#include <SHIESP32HW.h>
#include <SHIHardware.h>
#include <SPIFFS.h>

#include "SHISPIFFLoader.h"
const char *BOOT_NAME = "/boot.json";
const char *RUNTIME_NAME = "/runtime.json";

void setup() {
  std::string name = "SHIESP32Bootup";
  ets_printf("Loading SHIT\nTrying to load %s\n", RUNTIME_NAME);
  SHI::FactoryErrors error = bootstrapFromConfig(SPIFFS, RUNTIME_NAME);
  if (error == SHI::FactoryErrors::None) {
    SHI::hw->setup("RuntimeESP32");
    SHI_LOGINFO("Bootup success from runtime file");
    return;
  }
  ets_printf("Loading runtime failed with code:%d (%s)\nTrying to load %s",
             error, SHI::Factory::errorToString(error), BOOT_NAME);
  error = bootstrapFromConfig(SPIFFS, BOOT_NAME);
  if (error == SHI::FactoryErrors::None) {
    SHI::hw->setup("UnconfiguredESP32");
    SHI_LOGINFO(
        "Bootup success from bootstrap file, trying to load runtime config");
    HTTPClient http;
    auto hwConfig = SHI::hw->getConfigAs<SHI::ESP32HWConfig>();
    String url = String(hwConfig.baseURL.c_str()) +
                 SHI::hw->getNodeName().c_str() + ".json";
    http.begin(url);
    http.setConnectTimeout(hwConfig.CONNECT_TIMEOUT);
    http.setTimeout(hwConfig.DATA_TIMEOUT);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String newConfig = http.getString();
      if (writeConfigFile(SPIFFS, RUNTIME_NAME, newConfig)) {
        SHI::hw->resetWithReason("Runtime config file written", true);
      }
    } else {
      String msg = String("Failed to load runtime config from:") + url +
                   " Error was: " + String(httpCode) + " " +
                   http.errorToString(httpCode);
      SHI_LOGWARN(msg.c_str());
    }
  } else {
    ets_printf("Bootstrapping failed with code:%d (%s)\n", error,
               SHI::Factory::errorToString(error));
    while (true) {
      // Do nothing
    }
  }
}

void loop() { SHI::hw->loop(); }
