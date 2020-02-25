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

void SHI::RestCommunicator::newReading(const SHI::MeasurementBundle &reading,
                                       const SHI::Sensor &sensor) {
  if (!isConnected) return;
  auto sensorName = sensor.getName();
  SHI::hw->feedWatchdog();
  for (auto &&data : reading.data) {
    if (data.getDataState() == SHI::MeasurementDataState::VALID) {
      uploadInfo(SHI::hw->getNodeName(),
                 (String(sensorName) + data.getMetaData()->getName()).c_str(),
                 data.toTransmitString().c_str());
      SHI::hw->feedWatchdog();
    }
  }
  uploadInfo((String(SHI::hw->getNodeName()) + sensor.getName()).c_str(),
             STATUS_ITEM, STATUS_OK);
}

void SHI::RestCommunicator::newStatus(const SHI::Sensor &sensor,
                                      const char *message, bool isFatal) {
  if (!isConnected) {
    SHI_LOGINFO(("Not uploading: " + String(sensor.getName()) +
                 " as currently not connected")
                    .c_str());
    return;
  }
  uploadInfo(String(SHI::hw->getNodeName()) + String(sensor.getName()),
             STATUS_ITEM, message);
}

void SHI::RestCommunicator::newHardwareStatus(const char *message) {
  uploadInfo(SHI::hw->getNodeName(), STATUS_ITEM, message);
}

void SHI::RestCommunicator::uploadInfo(String valueName, String item,
                                       String value) {
  SHI_LOGINFO((valueName + " " + item + " " + value).c_str());
  bool tryHard = false;
  if (item == STATUS_ITEM && value != STATUS_OK) {
    tryHard = true;
  }
  int retryCount = 0;
  do {
    HTTPClient http;
    http.begin("http://192.168.188.250:8080/rest/items/esp32" + valueName +
               item + "/state");
    http.setConnectTimeout(CONNECT_TIMEOUT);
    http.setTimeout(DATA_TIMEOUT);
    int httpCode = http.PUT(value);
    printError(&http, httpCode);
    http.end();
    if (httpCode == 202) return;  // Either return early or try until success
    retryCount++;
    SHI::hw->feedWatchdog();
  } while (tryHard && retryCount < 15);
}

void SHI::RestCommunicator::printError(HTTPClient *http, int httpCode) {
  // httpCode will be negative on error
  if (httpCode > 0) {
    if (httpCode < 200 || httpCode > 299) httpErrorCount++;
    httpCount++;
    // HTTP header has been send and Server response header has been handled
    SHI_LOGINFO(("response:" + String(httpCode, 10)).c_str());

    if (httpCode == HTTP_CODE_OK) {
      /// String payload = http->getString();
      // if (!payload.isEmpty())
      // SHI_LOGINFO( "Response payload was:" + payload);
    }
  } else {
    errorCount++;
    // ets_printf(http.errorToString(httpCode).c_str());
    SHI_LOGINFO(("Failed " + String(httpCode, 10)).c_str());
  }
}

std::vector<std::pair<const char *, const char *>>
SHI::RestCommunicator::getStatistics() {
  return {{"httpFatalErrorCount", String(errorCount).c_str()},
          {"httpErrorCount", String(httpErrorCount).c_str()},
          {"httpCount", String(httpCount).c_str()}};
}
