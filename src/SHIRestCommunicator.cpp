/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIRestCommunicator.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include <string>

#include "SHIHardware.h"
#include "SHISensor.h"

namespace {

const int CONNECT_TIMEOUT = 500;
const int DATA_TIMEOUT = 1000;
const String OHREST = "OpenhabRest";
static bool endsWith(const std::string &str, const std::string &suffix) {
  return str.size() >= suffix.size() &&
         0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}
}  // namespace

void SHI::RestCommunicator::newReading(const SHI::MeasurementBundle &reading) {
  if (!isConnected) return;
  SHI::hw->feedWatchdog();
  for (auto &&data : reading.data) {
    if (data.getDataState() == SHI::MeasurementDataState::VALID) {
      uploadInfo(data.getMetaData()->getQualifiedName("_"),
                 data.stringRepresentation);
      SHI::hw->feedWatchdog();
    }
  }
}

void SHI::RestCommunicator::newStatus(const SHI::SHIObject &sensor,
                                      const char *message, bool isFatal) {
  if (!isConnected) {
    SHI_LOGINFO(("Not uploading: " + String(sensor.getName()) +
                 " as currently not connected")
                    .c_str());
    return;
  }
  uploadInfo(
      std::string(SHI::hw->getNodeName()) + sensor.getName() + STATUS_ITEM,
      std::string(message), true);
}

void SHI::RestCommunicator::newHardwareStatus(const char *message) {
  uploadInfo(std::string(SHI::hw->getNodeName()) + STATUS_ITEM,
             std::string(message), true);
}

void SHI::RestCommunicator::uploadInfo(const std::string &item,
                                       const std::string &value,
                                       bool tryHard = false) {
  SHI_LOGINFO((item + " " + value).c_str());
  int retryCount = 0;
  do {
    HTTPClient http;
    http.begin(String("http://192.168.188.250:8080/rest/items/") + prefix +
               item.c_str() + "/state");
    http.setConnectTimeout(CONNECT_TIMEOUT);
    http.setTimeout(DATA_TIMEOUT);
    int httpCode = http.PUT(String(value.c_str()));
    printError(&http, httpCode);
    http.end();
    if (httpCode >= 200 && httpCode < 300)
      return;  // Either return early or try until success
    retryCount++;
    SHI::hw->feedWatchdog();
  } while (tryHard && retryCount < 15);
}

void SHI::RestCommunicator::printError(HTTPClient *http, int httpCode) {
  // httpCode will be negative on error
  if (httpCode > 0) {
    if (httpCode < 200 || httpCode > 299) {
      httpErrorCount++;
      SHI_LOGWARN(("response:" + String(httpCode, 10)).c_str());
    }
    httpCount++;
  } else {
    errorCount++;
    SHI_LOGINFO(("Failed " + String(httpCode, 10)).c_str());
  }
}

std::vector<std::pair<std::string, std::string>>
SHI::RestCommunicator::getStatistics() {
  return {{"httpFatalErrorCount", String(errorCount).c_str()},
          {"httpErrorCount", String(httpErrorCount).c_str()},
          {"httpCount", String(httpCount).c_str()}};
}
