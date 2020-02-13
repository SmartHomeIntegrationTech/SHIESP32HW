/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIRestCommunicator.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include "SHIHardware.h"
#include "SHISensor.h"

namespace {

const int CONNECT_TIMEOUT = 500;
const int DATA_TIMEOUT = 1000;
const String STATUS_ITEM = "Status";
const String STATUS_OK = "OK";
const String OHREST = "OpenhabRest";

}  // namespace

void SHI::RestCommunicator::newReading(SHI::SensorReadings &reading,
                                       SHI::Sensor &sensor) {
  if (!isConnected) return;
  auto sensorName = sensor.getName();
  SHI::hw->feedWatchdog();
  for (auto &&data : reading.data) {
    if (data->type != SHI::SensorDataType::NO_DATA) {
      uploadInfo(SHI::hw->getNodeName(), sensorName + data->name,
                 data->toTransmitString(*data));
      SHI::hw->feedWatchdog();
    }
  }
  uploadInfo(SHI::hw->getNodeName() + sensor.getName(), STATUS_ITEM, STATUS_OK);
}

void SHI::RestCommunicator::newStatus(SHI::Sensor &sensor, String message,
                                      bool isFatal) {
  if (!isConnected) {
    SHI::hw->logInfo(
        name, __func__,
        "Not uploading: " + sensor.getName() + " as currently not connected");
    return;
  }
  uploadInfo(SHI::hw->getNodeName() + sensor.getName(), STATUS_ITEM, message);
}

void SHI::RestCommunicator::newHardwareStatus(String message) {
  uploadInfo(SHI::hw->getNodeName(), STATUS_ITEM, message);
}

void SHI::RestCommunicator::uploadInfo(String name, String item, String value) {
  SHI::hw->logInfo(name, __func__, name + " " + item + " " + value);
  bool tryHard = false;
  if (item == STATUS_ITEM && value != STATUS_OK) {
    tryHard = true;
  }
  int retryCount = 0;
  do {
    HTTPClient http;
    http.begin("http://192.168.188.250:8080/rest/items/esp32" + name + item +
               "/state");
    http.setConnectTimeout(CONNECT_TIMEOUT);
    http.setTimeout(DATA_TIMEOUT);
    int httpCode = http.PUT(value);
    printError(http, httpCode);
    http.end();
    if (httpCode == 202) return;  // Either return early or try until success
    retryCount++;
    SHI::hw->feedWatchdog();
  } while (tryHard && retryCount < 15);
}

void SHI::RestCommunicator::printError(HTTPClient &http, int httpCode) {
  // httpCode will be negative on error
  if (httpCode > 0) {
    if (httpCode < 200 || httpCode > 299) httpErrorCount++;
    httpCount++;
    // HTTP header has been send and Server response header has been handled
    SHI::hw->logInfo(name, __func__, "response:" + httpCode);

    if (httpCode == HTTP_CODE_OK) {
      /// String payload = http.getString();
      // if (!payload.isEmpty())
      // SHI::hw->logInfo(name, __func__, "Response payload was:" + payload);
    }
  } else {
    errorCount++;
    // ets_printf(http.errorToString(httpCode).c_str());
    SHI::hw->logInfo(name, __func__, "Failed " + httpCode);
  }
}

std::vector<std::pair<String, String>> SHI::RestCommunicator::getStatistics() {
  return {{"httpFatalErrorCount", String(errorCount)},
          {"httpErrorCount", String(httpErrorCount)},
          {"httpCount", String(httpCount)}};
}
