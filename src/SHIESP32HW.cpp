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
#include <time.h>

#include <cstring>
#include <map>
#include <vector>

namespace {

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

PROGMEM const std::string RESET_SOURCE[] = {
    "NO_MEAN",          "POWERON_RESET",    "SW_RESET",
    "OWDT_RESET",       "DEEPSLEEP_RESET",  "SDIO_RESET",
    "TG0WDT_SYS_RESET", "TG1WDT_SYS_RESET", "RTCWDT_SYS_RESET",
    "INTRUSION_RESET",  "TGWDT_CPU_RESET",  "SW_CPU_RESET",
    "RTCWDT_CPU_RESET", "EXT_CPU_RESET",    "RTCWDT_BROWN_OUT_RESET",
    "RTCWDT_RTC_RESET"};

const char *CONFIG = "wifiConfig";
const uint32_t CONST_MARKER = 0xCAFEBABE;

void IRAM_ATTR resetModule() {
  ets_printf("Watchdog bit, reboot\n");
  SHI::hw->resetWithReason("Watchdog triggered", false);
  esp_restart();
}

}  // namespace

SHI::Hardware *SHI::hw = nullptr;

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
const String internalVersion = String(SHI::MAJOR_VERSION, 10) + "." +
                               String(SHI::MINOR_VERSION, 10) + "." +
                               String(SHI::PATCH_VERSION, 10);
const char *SHI::VERSION = internalVersion.c_str();

void SHI::ESP32HW::errLeds(void) {
  // Set pin mode
  if (hwConfig.ERR_LED != -1) {
    pinMode(hwConfig.ERR_LED, OUTPUT);
    delay(500);
    digitalWrite(hwConfig.ERR_LED, HIGH);
    delay(500);
    digitalWrite(hwConfig.ERR_LED, LOW);
  } else {
    delay(1000);
  }
}

void SHI::ESP32HW::loop() {
  statusMessage = SHI::STATUS_OK;
  feedWatchdog();
  uint32_t start = millis();
  wifiIsConntected();
  internalLoop();
  uint32_t diff = millis() - start;
  averageSensorLoopDuration = ((averageSensorLoopDuration * 9) + diff) / 10.;
  if (diff < 1000) delay(diff);
}

void SHI::ESP32HW::setupWatchdog() {
  timer = timerBegin(0, 80, true);                            // timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);            // attach callback
  timerAlarmWrite(timer, hwConfig.wdtTimeout * 1000, false);  // set time in us
  timerAlarmEnable(timer);
}

void SHI::ESP32HW::feedWatchdog() {
  timerWrite(timer, 0);  // reset timer (feed watchdog)
}

void SHI::ESP32HW::disableWatchdog() { timerEnd(timer); }

std::string SHI::ESP32HW::getResetReason() { return config.resetReason; }

void SHI::ESP32HW::resetWithReason(const std::string &reason,
                                   bool restart = true) {
  std::memcpy(config.resetReason, reason.c_str(),
              sizeof(config.resetReason) - 1);
  config.resetReason[sizeof(config.resetReason) - 1] = 0;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
  if (restart) {
    SHI_LOGINFO(std::string("Restarting:") + reason);
    delay(100);
    ESP.restart();
  }
}

void SHI::ESP32HW::resetConfig() {
  config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
}

void SHI::ESP32HW::printConfig() {
  SHI_LOGINFO("IP address:  " +
              std::string(IPAddress(config.local_IP).toString().c_str()));
  SHI_LOGINFO("Subnet Mask: " + std::string(String(config.subnet, 16).c_str()));
  SHI_LOGINFO("Gateway IP:  " +
              std::string(IPAddress(config.gateway).toString().c_str()));
  SHI_LOGINFO("Canary:      " + std::string(String(config.canary, 16).c_str()));
  SHI_LOGINFO("Name:        " + std::string(config.name));
  SHI_LOGINFO("Reset reason:" + std::string(config.resetReason));
}

bool SHI::ESP32HW::updateNodeName() {
  HTTPClient http;
  String mac = WiFi.macAddress();
  mac.replace(':', '_');
  http.begin(String(hwConfig.baseURL.c_str()) + mac);
  http.setConnectTimeout(hwConfig.CONNECT_TIMEOUT);
  http.setTimeout(hwConfig.DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String newName = http.getString();
    newName.replace('\n', '\0');
    newName.trim();
    if (newName.length() == 0) return false;
    newName.toCharArray(config.name, sizeof(config.name));
    SHI_LOGINFO("Recevied new Name:" + std::string(newName.c_str()));
    return true;
  } else {
    SHI_LOGINFO("Failed to update name for mac:" + std::string(mac.c_str()));
  }
  return false;
}

std::string SHI::ESP32HW::getNodeName() { return config.name; }
const std::string SHI::ESP32HW::getName() const { return config.name; }

void SHI::ESP32HW::wifiDisconnected(WiFiEventInfo_t info) {
  SHI_LOGINFO("WiFi lost connection. Reason: " + info.disconnected.reason);
  for (auto &&comm : communicators) {
    comm->networkDisconnected();
  }
}

void SHI::ESP32HW::wifiConnected() {
  SHI_LOGINFO("Wifi connected");
  for (auto &&comm : communicators) {
    comm->networkConnected();
  }
}

void SHI::ESP32HW::setupWifiFromConfig(const std::string &defaultName) {
  IPAddress primaryDNS;
  primaryDNS.fromString(hwConfig.primaryDNS.c_str());  // optional
  IPAddress secondaryDNS;
  secondaryDNS.fromString(hwConfig.secondaryDNS.c_str());  // optional
  configPrefs.begin(CONFIG);
  configPrefs.getBytes(CONFIG, &config, sizeof(config_t));
  if (config.canary == CONST_MARKER) {
    SHI_LOGINFO("Restoring config from memory");
    printConfig();
    WiFi.setHostname(config.name);
    if (!WiFi.config(config.local_IP, config.gateway, config.subnet, primaryDNS,
                     secondaryDNS)) {
      SHI_LOGINFO("STA Failed to configure");
    }
  } else {
    SHI_LOGINFO("Canary mismatch, stored: " +
                std::string(String(config.canary, 16).c_str()));
    auto res =
        snprintf(config.name, sizeof(config.name), "%s", defaultName.c_str());
    SHI_LOGINFO(std::string("Config name is now:") + config.name + " " +
                std::string(String(res, 10).c_str()));
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
  WiFi.begin(hwConfig.ssid.c_str(), hwConfig.password.c_str());
}

void SHI::ESP32HW::initialWifiConnect() {
  while (WiFi.status() != WL_CONNECTED) {
    if (connectCount > hwConfig.reconnectAttempts) {
      ESP.restart();
    }
    delay(hwConfig.reconnectDelay);
    SHI_LOGINFO(".");
    connectCount++;
  }
  feedWatchdog();
  wifiConnected();
}

void SHI::ESP32HW::storeWifiConfig() {
  if (config.canary != CONST_MARKER && updateNodeName()) {
    SHI_LOGINFO("Storing config");
    config.local_IP = WiFi.localIP();
    config.gateway = WiFi.gatewayIP();
    config.subnet = WiFi.subnetMask();
    // config.name is set by updateNodeName
    WiFi.setHostname(config.name);
    resetWithReason("Fresh-reset", false);
    config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    SHI_LOGINFO("ESP Mac Address: " + std::string(WiFi.macAddress().c_str()));
    printConfig();
  }
}

void SHI::ESP32HW::setup(const std::string &defaultName) {
  setupWatchdog();
  feedWatchdog();

  setupWifiFromConfig(defaultName);
  SHI_LOGINFO("Connecting to " + hwConfig.ssid);

  uint32_t intialWifiConnectStart = millis();
  initialWifiConnect();
  storeWifiConfig();
  initialWifiConnectTime = millis() - intialWifiConnectStart;
  statusMessage =
      std::string("STARTED: ") + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
      RESET_SOURCE[rtc_get_reset_reason(1)] + " " + config.resetReason;
  feedWatchdog();
  configTime(hwConfig.gmtOffset_sec, hwConfig.daylightOffset_sec,
             hwConfig.ntpServer.c_str());
  feedWatchdog();
  uint32_t sensorSetupStart = millis();
  setupSensors();
  sensorSetupTime = millis() - sensorSetupStart;
  uint32_t commSetupStart = millis();
  setupCommunicators();
  commSetupTime = millis() - commSetupStart;
}

void SHI::ESP32HW::log(const String &msg) { debugSerial->println(msg); }
void SHI::ESP32HW::log(const std::string &msg) {
  debugSerial->println(msg.c_str());
}

int64_t SHI::ESP32HW::getEpochInMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

bool SHI::ESP32HW::wifiIsConntected() {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    feedWatchdog();
    if (retryCount > 6) {
      resetWithReason(std::string("Retry count for Wifi exceeded") +
                      std::string(String(WiFi.status()).c_str()));
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(hwConfig.ssid.c_str(), hwConfig.password.c_str());
    retryCount++;
    delay(retryCount * 1000);
  }
  uint32_t diff = millis() - start;
  averageConnectDuration = ((averageConnectDuration * 9) + diff) / 10.;
  retryCount = 0;
  return true;
}

std::vector<std::pair<std::string, std::string>> SHI::ESP32HW::getStatistics() {
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
