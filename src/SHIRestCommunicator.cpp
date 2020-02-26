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
}  // namespace

void SHI::RestCommunicator::newReading(const SHI::MeasurementBundle &reading) {
  if (!isConnected) {
    missedConnectionCount++;
    return;
  }
  SHI::hw->feedWatchdog();
  for (auto &&data : reading.data) {
    if (data.getDataState() == SHI::MeasurementDataState::VALID) {
      uploadInfo(data.getMetaData()->getQualifiedName("_"),
                 data.stringRepresentation);
      SHI::hw->feedWatchdog();
    }
  }
}

void SHI::RestCommunicator::newStatus(const SHI::Measurement &status,
                                      SHIObject *src) {
  if (!isConnected) {
    SHI_LOGINFO(("Not uploading: " +
                 String(status.getMetaData()->getQualifiedName().c_str()) +
                 " as currently not connected")
                    .c_str());
    missedConnectionCount++;
    return;
  }
  uploadInfo(status.getMetaData()->getQualifiedName("_"),
             status.stringRepresentation, true);
}

void SHI::RestCommunicator::uploadInfo(const std::string &item,
                                       const std::string &value, bool tryHard) {
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
    statusMessage =
        ("Last transmit failed with " + String(httpCode, 10)).c_str();
  }
}

std::vector<std::pair<std::string, std::string>>
SHI::RestCommunicator::getStatistics() {
  return {{"httpFatalErrorCount", String(errorCount).c_str()},
          {"httpErrorCount", String(httpErrorCount).c_str()},
          {"missedConnectionCount", String(missedConnectionCount).c_str()},
          {"httpCount", String(httpCount).c_str()}};
}
