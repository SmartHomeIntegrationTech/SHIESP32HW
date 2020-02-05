#include "WifiBase.h"
#include "oled/SSD1306Wire.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cstring>
#include <map>
#include <rom/rtc.h>
#include <vector>

#ifndef BUILTIN_LED
#warning "Did not define BUILTIN_LED"
#define BUILTIN_LED -1
#endif

SHI::HWBase SHI::hw;
SHI::config_t SHI::config;

namespace {
#ifndef NO_SERIAL
class HarwareSHIPrinter : public SHI::SHIPrinter {
  void begin(int baudRate) { Serial.begin(baudRate); };
  size_t write(uint8_t data) { return Serial.write(data); };
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

const int wdtTimeout = 15000; // time in ms to trigger the watchdog
hw_timer_t *timer = NULL;

#ifdef HAS_DISPLAY
SSD1306Wire display =
    SSD1306Wire(0x3c, SDA_OLED, SCL_OLED, RST_OLED, GEOMETRY_128_64);
static String displayLineBuf[7] = {
    "Temperatur:", "n/a", "Luftfeuchtigkeit:", "n/a", "Luftqualität:",
    "n/a",         ""};
bool displayUpdated = false;
#endif

const int CONNECT_TIMEOUT = 500;
const int DATA_TIMEOUT = 1000;

void IRAM_ATTR resetModule() {
  ets_printf("Watchdog bit, reboot\n");
  SHI::hw.resetWithReason("Watchdog triggered", false);
  esp_restart();
}

} // namespace

void SHI::HWBase::errLeds(void) {
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

void SHI::HWBase::loop() {
  feedWatchdog();
  unsigned long start = millis();
  if (wifiIsConntected()) {
    for (auto &&channel : channels) {
      auto sensor = channel->sensor;
      auto reading = sensor->readSensor();
      logInfo(name, __func__,
              "Reading sensor:" + sensor->getName() +
                  " on channel:" + channel->name);
      if (reading == nullptr) {
        auto status = sensor->getStatusMessage();
        auto isFatal = sensor->errorIsFatal();
        for (auto &&comm : communicators) {
          comm->newStatus(*channel, status, isFatal);
        }
        if (isFatal) {
          logError(name, __func__,
                   "Sensor " + channel->name + sensor->getName() +
                       " reported error " + status);
        } else {
          logWarn(name, __func__,
                  "Sensor " + channel->name + sensor->getName() +
                      " reported warning " + status);
        }
      } else {
        for (auto &&comm : communicators) {
          comm->newReading(*reading, *channel);
        }
      }
    }
#ifdef HAS_DISPLAY
    if (displayUpdated) {
      display.clear();
      for (int i = 1; i < 6; i += 2)
        display.drawString(90, (i / 2) * 13, displayLineBuf[i]);
      for (int i = 0; i < 6; i += 2)
        display.drawString(0, (i / 2) * 13, displayLineBuf[i]);
      display.drawStringMaxWidth(0, 3 * 13, 128, displayLineBuf[6]);
      display.display();
      displayUpdated = false;
    }
#endif
  }
  for (auto &&comm : communicators) {
    comm->loopCommunication();
  }
  int diff = millis() - start;
  if (diff < 1000)
    delay(diff);
}

void SHI::HWBase::setDisplayBrightness(uint8_t value) {
#ifdef HAS_DISPLAY
  display.setBrightness(value);
#endif
}

void SHI::HWBase::setupWatchdog() {
  timer = timerBegin(0, 80, true);                  // timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  // attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); // set time in us
  timerAlarmEnable(timer);
}

void SHI::HWBase::disableWatchdog() { timerEnd(timer); }

void SHI::HWBase::resetWithReason(const char *reason, bool restart = true) {
  std::memcpy(SHI::config.resetReason, reason,
              sizeof(SHI::config.resetReason) - 1);
  configPrefs.putBytes(CONFIG, &config, sizeof(SHI::config_t));
  if (restart) {
    logInfo(name, __func__, "Restarting:" + String(reason));
    delay(100);
    ESP.restart();
  }
}

void SHI::HWBase::printConfig() {
  SHI::hw.logInfo(name, __func__,
                  "IP address:  " + String(SHI::config.local_IP, 16));
  SHI::hw.logInfo(name, __func__,
                  "Subnet Mask: " + String(SHI::config.subnet, 16));
  SHI::hw.logInfo(name, __func__,
                  "Gateway IP:  " + String(SHI::config.gateway, 16));
  SHI::hw.logInfo(name, __func__,
                  "Canary:      " + String(SHI::config.canary, 16));
  SHI::hw.logInfo(name, __func__, "Name:        " + String(SHI::config.name));
  SHI::hw.logInfo(name, __func__,
                  "Reset reason:" + String(SHI::config.resetReason));
}

bool SHI::HWBase::updateNodeName() {
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
    if (newName.length() == 0)
      return false;
    newName.toCharArray(SHI::config.name, sizeof(SHI::config.name));
    SHI::hw.logInfo(name, __func__, "Recevied new Name:" + newName);
    return true;
  } else {
    SHI::hw.logInfo(name, __func__, "Failed to update name for mac:" + mac);
  }
  return false;
}

String SHI::HWBase::getNodeName() { return String(SHI::config.name); }

void SHI::HWBase::wifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  logInfo(name, __func__,
          "WiFi lost connection. Reason: " + info.disconnected.reason);
  for (auto &&comm : communicators) {
    comm->wifiDisconnected();
  }
}

void SHI::HWBase::wifiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  logInfo(name, __func__, "Wifi connected");
  for (auto &&comm : communicators) {
    comm->wifiConnected();
  }
}

void SHI::HWBase::resetConfig() {
  config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
}

void SHI::HWBase::setup(String defaultName) {
  IPAddress primaryDNS(192, 168, 188, 250); // optional
  IPAddress secondaryDNS(192, 168, 188, 1); // optional
  setupWatchdog();
  feedWatchdog();
  debugSerial = &shiSerial;
  Serial.begin(115200);

#ifdef HAS_DISPLAY
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "OLED initial done!");
  display.display();
  feedWatchdog();
#endif
  configPrefs.begin(CONFIG);
  configPrefs.getBytes(CONFIG, &config, sizeof(config_t));
  if (SHI::config.canary == CONST_MARKER) {
    logInfo(name, __func__, "Restoring config from memory");
    printConfig();
    WiFi.setHostname(SHI::config.name);
    if (!WiFi.config(SHI::config.local_IP, SHI::config.gateway,
                     SHI::config.subnet, primaryDNS, secondaryDNS)) {
      logInfo(name, __func__, "STA Failed to configure");
    }
  } else {
    logInfo(name, __func__,
            "Canary mismatch, stored: " + String(SHI::config.canary, 16));
    defaultName.toCharArray(SHI::config.name, sizeof(SHI::config.name));
  }

  logInfo(name, __func__, "Connecting to " + String(ssid));

  feedWatchdog();
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    logInfo(name, "Wifi", "Event:" + String(event) + " triggered");
  });
  WiFi.onEvent([this](WiFiEvent_t event,
                      WiFiEventInfo_t info) { wifiDisconnected(event, info); },
               SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent([this](WiFiEvent_t event,
                      WiFiEventInfo_t info) { wifiDisconnected(event, info); },
               SYSTEM_EVENT_STA_LOST_IP);
  WiFi.onEvent([this](WiFiEvent_t event,
                      WiFiEventInfo_t info) { wifiConnected(event, info); },
               SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent([this](WiFiEvent_t event,
                      WiFiEventInfo_t info) { wifiConnected(event, info); },
               SYSTEM_EVENT_STA_GOT_IP);
  WiFi.begin(ssid, password);

  int connectCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (connectCount > 10) {
      ESP.restart();
    }
    delay(500);
    logInfo(name, __func__, ".");
    connectCount++;
  }
  feedWatchdog();
  logInfo(name, __func__, "WiFi connected!");
  if (SHI::config.canary != CONST_MARKER && updateNodeName()) {
    logInfo(name, __func__, "Storing config");
    SHI::config.local_IP = WiFi.localIP();
    SHI::config.gateway = WiFi.gatewayIP();
    SHI::config.subnet = WiFi.subnetMask();
    // SHI::config.name is set by updateNodeName
    WiFi.setHostname(SHI::config.name);
    resetWithReason("Fresh-reset", false);
    SHI::config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    logInfo(name, __func__, "ESP Mac Address: " + WiFi.macAddress());
    printConfig();
  }
  auto hwStatus = "STARTED: " + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
                  RESET_SOURCE[rtc_get_reset_reason(1)] + " " +
                  String(SHI::config.resetReason);
  for (auto &&comm : communicators) {
    comm->newHardwareStatus(hwStatus);
  }
  feedWatchdog();

  for (auto &&comm : communicators) {
    comm->setupCommunication();
  }

  for (auto &&channel : channels) {
    auto sensor = channel->sensor;
    String name = sensor->getName();
    logInfo(name, __func__, "Setting up: " + name);
    if (!sensor->setupSensor()) {
      logInfo(name, __func__,
              "Something went wrong when setting up sensor:" + name +
                  channel->name + " " + sensor->getStatusMessage());
      while (1) {
        errLeds();
      }
    }
    feedWatchdog();
    logInfo(name, __func__, "Setup done of: " + name);
  }
}

void SHI::HWBase::log(String msg) { debugSerial->println(msg); }

bool SHI::HWBase::wifiIsConntected() {
  static int retryCount = 0;
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
  retryCount = 0;
  return true;
}

void SHI::HWBase::feedWatchdog() {
  timerWrite(timer, 0); // reset timer (feed watchdog)
}
