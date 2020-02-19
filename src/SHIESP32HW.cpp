/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIESP32HW.h"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <rom/rtc.h>

#include <cstring>
#include <map>
#include <vector>

#ifndef BUILTIN_LED
#warning "Did not define BUILTIN_LED"
#define BUILTIN_LED -1
#endif

namespace {

SHI::ESP32HW instance;

#ifndef NO_SERIAL
class HarwareSHIPrinter : public SHI::SHIPrinter {
  void begin(int baudRate) { Serial.begin(baudRate); }
  size_t write(uint8_t data) { return Serial.write(data); }
};
HarwareSHIPrinter shiSerial;
#else
class NullSHIPrinter : public SHI::SHIPrinter {
 public:
  size_t write(uint8_t) { return 1; }
  void begin(int baudRate) {}
};
NullSHIPrinter shiSerial;
#endif

PROGMEM const String RESET_SOURCE[] = {
    "NO_MEAN",          "POWERON_RESET",    "SW_RESET",
    "OWDT_RESET",       "DEEPSLEEP_RESET",  "SDIO_RESET",
    "TG0WDT_SYS_RESET", "TG1WDT_SYS_RESET", "RTCWDT_SYS_RESET",
    "INTRUSION_RESET",  "TGWDT_CPU_RESET",  "SW_CPU_RESET",
    "RTCWDT_CPU_RESET", "EXT_CPU_RESET",    "RTCWDT_BROWN_OUT_RESET",
    "RTCWDT_RTC_RESET"};

const char *ssid = "Elfenburg";
const char *password = "fe-shnyed-olv-ek";
const char *CONFIG = "wifiConfig";
const uint32_t CONST_MARKER = 0xCAFEBABE;

const int wdtTimeout = 15000;  // time in ms to trigger the watchdog

const int CONNECT_TIMEOUT = 500;
const int DATA_TIMEOUT = 1000;

void IRAM_ATTR resetModule() {
  ets_printf("Watchdog bit, reboot\n");
  SHI::hw->resetWithReason("Watchdog triggered", false);
  esp_restart();
}

}  // namespace

SHI::Hardware *SHI::hw = &instance;

#ifndef VER_MAJ
#error "Major version undefined"
#endif
#ifndef VER_MIN
#error "Minor version undefined"
#endif
#ifndef VER_PAT
#error "Patch version undefined"
#endif
const uint8_t SHI::MAJOR_VERSION = VER_MAJ;
const uint8_t SHI::MINOR_VERSION = VER_MIN;
const uint8_t SHI::PATCH_VERSION = VER_PAT;
const char *SHI::VERSION =
    (String(SHI::MAJOR_VERSION, 10) + "." + String(SHI::MINOR_VERSION, 10) +
     "." + String(SHI::PATCH_VERSION, 10))
        .c_str();

void SHI::ESP32HW::errLeds(void) {
  // Set pin mode
  if (BUILTIN_LED != -1) {
    pinMode(BUILTIN_LED, OUTPUT);
    delay(500);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(500);
    digitalWrite(BUILTIN_LED, LOW);
  } else {
    delay(1000);
  }
}

void SHI::ESP32HW::loop() {
  feedWatchdog();
  uint32_t start = millis();
  bool sensorHasFatalError = false;
  if (wifiIsConntected()) {
    for (auto &&sensor : sensors) {
      auto reading = sensor->readSensor();
      logInfo(name, __func__, "Reading sensor:" + String(sensor->getName()));
      if (reading == nullptr) {
        auto status = sensor->getStatusMessage();
        auto isFatal = sensor->errorIsFatal();
        for (auto &&comm : communicators) {
          comm->newStatus(*sensor, status, isFatal);
        }
        if (isFatal) {
          sensorHasFatalError = true;
          logError(name, __func__,
                   "Sensor " + String(sensor->getName()) + " reported error " +
                       status);
        } else {
          logWarn(name, __func__,
                  "Sensor " + String(sensor->getName()) + " reported warning " +
                      status);
        }
      } else {
        for (auto &&comm : communicators) {
          comm->newReading(*reading, *sensor);
        }
      }
    }
  }
  for (auto &&comm : communicators) {
    comm->loopCommunication();
  }
  uint32_t diff = millis() - start;
  averageSensorLoopDuration = ((averageSensorLoopDuration * 9) + diff) / 10.;

  while (sensorHasFatalError) {
    errLeds();
  }
  if (diff < 1000) delay(diff);
}

void SHI::ESP32HW::setupWatchdog() {
  timer = timerBegin(0, 80, true);                   // timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);   // attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false);  // set time in us
  timerAlarmEnable(timer);
}

void SHI::ESP32HW::feedWatchdog() {
  timerWrite(timer, 0);  // reset timer (feed watchdog)
}

void SHI::ESP32HW::disableWatchdog() { timerEnd(timer); }

const char *SHI::ESP32HW::getResetReason() { return config.resetReason; }

void SHI::ESP32HW::resetWithReason(const char *reason, bool restart = true) {
  std::memcpy(config.resetReason, reason, sizeof(config.resetReason) - 1);
  config.resetReason[sizeof(config.resetReason) - 1] = 0;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
  if (restart) {
    logInfo(name, __func__, "Restarting:" + String(reason));
    delay(100);
    ESP.restart();
  }
}

void SHI::ESP32HW::resetConfig() {
  config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
}

void SHI::ESP32HW::printConfig() {
  logInfo(name, __func__, "IP address:  " + String(config.local_IP, 16));
  logInfo(name, __func__, "Subnet Mask: " + String(config.subnet, 16));
  logInfo(name, __func__, "Gateway IP:  " + String(config.gateway, 16));
  logInfo(name, __func__, "Canary:      " + String(config.canary, 16));
  logInfo(name, __func__, "Name:        " + String(config.name));
  logInfo(name, __func__, "Reset reason:" + String(config.resetReason));
}

bool SHI::ESP32HW::updateNodeName() {
  HTTPClient http;
  String mac = WiFi.macAddress();
  mac.replace(':', '_');
  http.begin("http://192.168.188.250/esp/" + mac);
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String newName = http.getString();
    newName.replace('\n', '\0');
    newName.trim();
    if (newName.length() == 0) return false;
    newName.toCharArray(config.name, sizeof(config.name));
    logInfo(name, __func__, "Recevied new Name:" + newName);
    return true;
  } else {
    logInfo(name, __func__, "Failed to update name for mac:" + mac);
  }
  return false;
}

const char *SHI::ESP32HW::getNodeName() { return config.name; }

void SHI::ESP32HW::wifiDisconnected(WiFiEventInfo_t info) {
  logInfo(name, __func__,
          "WiFi lost connection. Reason: " + info.disconnected.reason);
  for (auto &&comm : communicators) {
    comm->wifiDisconnected();
  }
}

void SHI::ESP32HW::wifiConnected() {
  logInfo(name, __func__, "Wifi connected");
  for (auto &&comm : communicators) {
    comm->wifiConnected();
  }
}

void SHI::ESP32HW::setupSensors() {
  for (auto &&sensor : sensors) {
    String sensorName = sensor->getName();
    logInfo(name, __func__, "Setting up: " + sensorName);
    if (!sensor->setupSensor()) {
      logInfo(name, __func__,
              "Something went wrong when setting up sensor:" + sensorName +
                  " " + sensor->getStatusMessage());
      while (1) {
        errLeds();
      }
    }
    feedWatchdog();
    logInfo(name, __func__, "Setup done of: " + sensorName);
  }
}

void SHI::ESP32HW::setupWifiFromConfig(const char *defaultName) {
  IPAddress primaryDNS(192, 168, 188, 250);  // optional
  IPAddress secondaryDNS(192, 168, 188, 1);  // optional
  configPrefs.begin(CONFIG);
  configPrefs.getBytes(CONFIG, &config, sizeof(config_t));
  if (config.canary == CONST_MARKER) {
    logInfo(name, __func__, "Restoring config from memory");
    printConfig();
    WiFi.setHostname(config.name);
    if (!WiFi.config(config.local_IP, config.gateway, config.subnet, primaryDNS,
                     secondaryDNS)) {
      logInfo(name, __func__, "STA Failed to configure");
    }
  } else {
    logInfo(name, __func__,
            "Canary mismatch, stored: " + String(config.canary, 16));
    snprintf(config.name, sizeof(config.name), "%s", defaultName);
  }

  feedWatchdog();
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    logInfo(name, "Wifi", "Event:" + String(event) + " triggered");
  });
  WiFi.onEvent([this](WiFiEvent_t event,
                      WiFiEventInfo_t info) { wifiDisconnected(info); },
               SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent([this](WiFiEvent_t event,
                      WiFiEventInfo_t info) { wifiDisconnected(info); },
               SYSTEM_EVENT_STA_LOST_IP);
  WiFi.onEvent(
      [this](WiFiEvent_t event, WiFiEventInfo_t info) { wifiConnected(); },
      SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(
      [this](WiFiEvent_t event, WiFiEventInfo_t info) { wifiConnected(); },
      SYSTEM_EVENT_STA_GOT_IP);
  WiFi.begin(ssid, password);
}

void SHI::ESP32HW::initialWifiConnect() {
  while (WiFi.status() != WL_CONNECTED) {
    if (connectCount > 10) {
      ESP.restart();
    }
    delay(500);
    logInfo(name, __func__, ".");
    connectCount++;
  }
  feedWatchdog();
  wifiConnected();
}

void SHI::ESP32HW::storeWifiConfig() {
  if (config.canary != CONST_MARKER && updateNodeName()) {
    logInfo(name, __func__, "Storing config");
    config.local_IP = WiFi.localIP();
    config.gateway = WiFi.gatewayIP();
    config.subnet = WiFi.subnetMask();
    // config.name is set by updateNodeName
    WiFi.setHostname(config.name);
    resetWithReason("Fresh-reset", false);
    config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    logInfo(name, __func__, "ESP Mac Address: " + WiFi.macAddress());
    printConfig();
  }
}

void SHI::ESP32HW::setup(const char *defaultName) {
  setupWatchdog();
  feedWatchdog();
  debugSerial = &shiSerial;
  debugSerial->begin(115200);

  setupWifiFromConfig(defaultName);
  logInfo(name, __func__, "Connecting to " + String(ssid));

  uint32_t intialWifiConnectStart = millis();
  initialWifiConnect();
  storeWifiConfig();
  initialWifiConnectTime = millis() - intialWifiConnectStart;
  auto hwStatus =
      ("STARTED: " + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
       RESET_SOURCE[rtc_get_reset_reason(1)] + " " + String(config.resetReason))
          .c_str();
  uint32_t commSetupStart = millis();
  for (auto &&comm : communicators) {
    comm->setupCommunication();
    comm->newHardwareStatus(hwStatus);
  }
  commSetupTime = millis() - commSetupStart;
  feedWatchdog();
  uint32_t sensorSetupStart = millis();
  setupSensors();
  sensorSetupTime = millis() - sensorSetupStart;
}

void SHI::ESP32HW::log(const String &msg) { debugSerial->println(msg); }
void SHI::ESP32HW::log(const char *msg) { debugSerial->println(msg); }

bool SHI::ESP32HW::wifiIsConntected() {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    feedWatchdog();
    if (retryCount > 6) {
      resetWithReason("Retry count for Wifi exceeded" + WiFi.status());
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    retryCount++;
    delay(retryCount * 1000);
  }
  uint32_t diff = millis() - start;
  averageConnectDuration = ((averageConnectDuration * 9) + diff) / 10.;
  retryCount = 0;
  return true;
}

void SHI::ESP32HW::accept(SHI::Visitor &visitor) {
  visitor.visit(this);
  for (auto &&comm : communicators) {
    comm->accept(visitor);
  }
  for (auto &&sensor : sensors) {
    sensor->accept(visitor);
  }
}

std::vector<std::pair<const char *, const char *>>
SHI::ESP32HW::getStatistics() {
  return {
      {"connectCount", String(connectCount).c_str()},
      {"retryCount", String(retryCount).c_str()},
      {"initialWifiConnectTime", String(initialWifiConnectTime).c_str()},
      {"commSetupTime", String(commSetupTime).c_str()},
      {"sensorSetupTime", String(sensorSetupTime).c_str()},
      {"averageSensorLoopDuration", String(averageSensorLoopDuration).c_str()},
      {"averageConnectDuration", String(averageConnectDuration).c_str()},
  };
}
