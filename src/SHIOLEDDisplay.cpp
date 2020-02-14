/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIOLEDDisplay.h"

#include <oled/SSD1306Wire.h>

#include "SHIHardware.h"

namespace {
SSD1306Wire display =
    SSD1306Wire(0x3c, SDA_OLED, SCL_OLED, RST_OLED, GEOMETRY_128_64);
bool displayUpdated = false;
}  // namespace

void SHI::OLEDDisplay::setupCommunication() {
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "OLED initial done!");
  display.setBrightness(5);
  display.display();
  SHI::hw->feedWatchdog();
}

void SHI::OLEDDisplay::loopCommunication() {
  if (displayUpdated) {
    displayUpdated = false;
    display.clear();
    for (int i = 0; i < 6; i += 2)
      display.drawString(0, (i / 2) * 13, displayLineBuf[i]);
    for (int i = 1; i < 6; i += 2)
      display.drawString(90, (i / 2) * 13, displayLineBuf[i]);
    display.drawStringMaxWidth(0, 3 * 13, 128, displayLineBuf[6]);
    display.display();
  }
}

void SHI::OLEDDisplay::newReading(const SHI::SensorReadings &reading,
                                  const SHI::Sensor &sensor) {
  const String baseName = sensor.getName();
  for (auto &&data : reading.data) {
    auto sensorName = baseName + data->name;
    auto value = displayItems.find(sensorName);
    if (value != displayItems.end()) {
      displayLineBuf[(value->second * 2) + 1] = data->toTransmitString(*data);
      displayUpdated = true;
    }
  }
}

void SHI::OLEDDisplay::newStatus(const SHI::Sensor &sensor,
                                 const String &message, bool isFatal) {
  if (message != STATUS_OK) {
    displayLineBuf[6] = message;
    displayUpdated = true;
  }
}

void SHI::OLEDDisplay::newHardwareStatus(const String &message) {
  displayLineBuf[6] = "HW:" + message;
  displayUpdated = true;
}

void SHI::OLEDDisplay::setBrightness(uint8_t level) {
  display.setBrightness(level);
}
