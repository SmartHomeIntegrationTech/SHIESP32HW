#include "WifiBase.h"
#include "oled/SSD1306Wire.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <map>
#include <rom/rtc.h>
#include <vector>

PROGMEM const String RESET_SOURCE[] = {
    "NO_MEAN",          "POWERON_RESET",    "SW_RESET",
    "OWDT_RESET",       "DEEPSLEEP_RESET",  "SDIO_RESET",
    "TG0WDT_SYS_RESET", "TG1WDT_SYS_RESET", "RTCWDT_SYS_RESET",
    "INTRUSION_RESET",  "TGWDT_CPU_RESET",  "SW_CPU_RESET",
    "RTCWDT_CPU_RESET", "EXT_CPU_RESET",    "RTCWDT_BROWN_OUT_RESET",
    "RTCWDT_RTC_RESET"};

#ifndef BUILTIN_LED
#warning "Did not define BUILTIN_LED"
#define BUILTIN_LED -1
#endif

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

const char *ssid = "Elfenburg";
const char *password = "fe-shnyed-olv-ek";

AsyncUDP udpMulticast;

const char *CONFIG = "wifiConfig";
const uint32_t CONST_MARKER = 0xAFFEDEAD;
SHI::config_t SHI::config;
Preferences configPrefs;
bool doUpdate = false;

SHI::HWBase SHI::hw;

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

void IRAM_ATTR resetModule() {
  ets_printf("Watchdog bit, reboot\n");
  SHI::hw.resetWithReason("Watchdog triggered", false);
  esp_restart();
}

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
  for (auto &&channel : channels) {
    auto sensor = channel->sensor;
    auto reading = sensor->readSensor();
    if (reading == nullptr) {
      auto status = sensor->getStatusMessage();
      auto isFatal = sensor->errorIsFatal();
      for (auto &&comm : communicators) {
        comm->newStatus(*channel, status, isFatal);
      }
      if (isFatal) {
        logError("ESP32", __func__,
                 "Sensor " + channel->name + sensor->getName() +
                     " reported error " + status);
      } else {
        logWarn("ESP32", __func__,
                "Sensor " + channel->name + sensor->getName() +
                    " reported warning " + status);
      }
    } else {
      for (auto &&comm : communicators) {
        comm->newReading(*reading, *channel);
      }
    }
  }
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

void SHI::HWBase::resetWithReason(String reason, bool restart = true) {
  reason.toCharArray(SHI::config.resetReason, sizeof(SHI::config.resetReason));
  configPrefs.putBytes(CONFIG, &config, sizeof(SHI::config_t));
  if (restart) {
    logInfo("ESP32", __func__, "Restarting:" + reason);
    delay(100);
    ESP.restart();
  }
}

void SHI::HWBase::printConfig() {
  SHI::hw.logInfo("ESP32", __func__,
                  "IP address:  " + String(SHI::config.local_IP, 16));
  SHI::hw.logInfo("ESP32", __func__,
                  "Subnet Mask: " + String(SHI::config.subnet, 16));
  SHI::hw.logInfo("ESP32", __func__,
                  "Gateway IP:  " + String(SHI::config.gateway, 16));
  SHI::hw.logInfo("ESP32", __func__,
                  "Canary:      " + String(SHI::config.canary, 16));
  SHI::hw.logInfo("ESP32", __func__,
                  "Name:        " + String(SHI::config.name));
  SHI::hw.logInfo("ESP32", __func__,
                  "Reset reason:" + String(SHI::config.resetReason));
}

bool SHI::HWBase::updateNodeName() {
  HTTPClient http;
  String mac = WiFi.macAddress();
  mac.replace(':', '_');
  http.begin("http://192.168.188.250/esp/" + mac);
  http.setConnectTimeout(SHI::CONNECT_TIMEOUT);
  http.setTimeout(SHI::DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String newName = http.getString();
    newName.replace('\n', '\0');
    newName.trim();
    if (newName.length() == 0)
      return false;
    newName.toCharArray(SHI::config.name, sizeof(SHI::config.name));
    SHI::hw.logInfo("ESP32", __func__, "Recevied new Name:" + newName);
    return true;
  } else {
    SHI::hw.logInfo("ESP32", __func__, "Failed to update name for mac:" + mac);
  }
  return false;
}

void updateHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo("ESP32", __func__, "UPDATE called");
  packet.printf("OK UPDATE:%s", SHI::config.name);
  doUpdate = true;
}

void resetHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo("ESP32", __func__, "RESET called");
  packet.printf("OK RESET:%s", SHI::config.name);
  packet.flush();
  SHI::hw.resetWithReason("UDP RESET request");
}

void reconfHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo("ESP32", __func__, "RECONF called");
  SHI::config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &SHI::config, sizeof(SHI::config_t));
  packet.printf("OK RECONF:%s", SHI::config.name);
  packet.flush();
  SHI::hw.resetWithReason("UDP RECONF request");
}

void infoHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo("ESP32", __func__, "INFO called");
  packet.printf("OK INFO:%s\n"
                "Version:%s\n"
                "ResetReason:%s\n"
                "RunTimeInMillis:%lu\n"
                "ResetSource:%s:%s\n"
                "LocalIP:%s\n"
                "Mac:%s\n",
                SHI::config.name, SHI::VERSION.c_str(), SHI::config.resetReason,
                millis(), RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
                RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
                WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
}

void versionHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo("ESP32", __func__, "VERSION called");
  packet.printf("OK VERSION:%s\nVersion:%s", SHI::config.name,
                SHI::VERSION.c_str());
}

std::map<String, AuPacketHandlerFunction> registeredHandlers = {
    {"UPDATE", updateHandler},
    {"RECONF", reconfHandler},
    {"RESET", resetHandler},
    {"INFO", infoHandler},
    {"VERSION", versionHandler}};

void SHI::HWBase::addUDPPacketHandler(String trigger,
                                      AuPacketHandlerFunction handler) {
  registeredHandlers[trigger] = handler;
}

void handleUDPPacket(AsyncUDPPacket &packet) {
  const char *data = (const char *)(packet.data());
  if (packet.length() < 10) {
    auto handler = registeredHandlers[String(data)];
    if (handler != NULL) {
      handler(packet);
    }
  }
}

String SHI::HWBase::getNodeName() { return String(SHI::config.name); }

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
    logInfo("ESP32", __func__, "Restoring config from memory");
    printConfig();
    WiFi.setHostname(SHI::config.name);
    if (!WiFi.config(SHI::config.local_IP, SHI::config.gateway,
                     SHI::config.subnet, primaryDNS, secondaryDNS)) {
      logInfo("ESP32", __func__, "STA Failed to configure");
    }
  } else {
    debugSerial->printf("Canary mismatch, stored: %08X\n", SHI::config.canary);
    defaultName.toCharArray(SHI::config.name, sizeof(SHI::config.name));
  }

  debugSerial->print("Connecting to " + String(ssid));

  feedWatchdog();
  WiFi.begin(ssid, password);

  int connectCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (connectCount > 10) {
      ESP.restart();
    }
    delay(500);
    debugSerial->print(".");
    connectCount++;
  }
  feedWatchdog();
  logInfo("ESP32", __func__, "WiFi connected!");
  if (SHI::config.canary != CONST_MARKER && updateNodeName()) {
    logInfo("ESP32", __func__, "Storing config");
    SHI::config.local_IP = WiFi.localIP();
    SHI::config.gateway = WiFi.gatewayIP();
    SHI::config.subnet = WiFi.subnetMask();
    // SHI::config.name is set by updateNodeName
    WiFi.setHostname(SHI::config.name);
    resetWithReason("Fresh-reset", false);
    SHI::config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    logInfo("ESP32", __func__, "ESP Mac Address: " + WiFi.macAddress());
    printConfig();
  }
  auto hwStatus = "STARTED: " + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
                  RESET_SOURCE[rtc_get_reset_reason(1)] + " " +
                  String(SHI::config.resetReason);
  for (auto &&comm : communicators) {
    comm->newHardwareStatus(hwStatus);
  }
  feedWatchdog();
  if (udpMulticast.listenMulticast(IPAddress(239, 1, 23, 42), 2323)) {
    udpMulticast.onPacket(handleUDPPacket);
  }

  for (auto &&channel : channels) {
    auto sensor = channel->sensor;
    String name = sensor->getName();
    logInfo("ESP32", __func__, "Setting up: " + name);
    if (!sensor->setupSensor()) {
      logInfo("ESP32", __func__,
              "Something went wrong when setting up sensor:" + name +
                  channel->name + " " + sensor->getStatusMessage());
      while (1) {
        errLeds();
      }
    }
    feedWatchdog();
    logInfo("ESP32", __func__, "Setup done of: " + name);
  }
}

void updateProgress(size_t a, size_t b) {
  udpMulticast.printf("OK UPDATE:%s %u/%u", SHI::config.name, a, b);
  SHI::hw.feedWatchdog();
}

bool isUpdateAvailable() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" + String(SHI::config.name) +
             ".version");
  http.setConnectTimeout(SHI::CONNECT_TIMEOUT);
  http.setTimeout(SHI::DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String remoteVersion = http.getString();
    SHI::hw.logInfo("ESP32", __func__, "Remote version is:" + remoteVersion);
    return SHI::VERSION.compareTo(remoteVersion) < 0;
  }
  return false;
}

void startUpdate() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" + String(SHI::config.name) +
             ".bin");
  http.setConnectTimeout(SHI::CONNECT_TIMEOUT);
  http.setTimeout(SHI::DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    udpMulticast.printf("OK UPDATE:%s Starting", SHI::config.name);
    int size = http.getSize();
    if (size < 0) {
      udpMulticast.printf("ERR UPDATE:%s Abort, no size", SHI::config.name);
      return;
    }
    if (!Update.begin(size)) {
      udpMulticast.printf("ERR UPDATE:%s Abort, not enough space",
                          SHI::config.name);
      return;
    }
    Update.onProgress(updateProgress);
    size_t written = Update.writeStream(http.getStream());
    if (written == size) {
      udpMulticast.printf("OK UPDATE:%s Finishing", SHI::config.name);
      if (!Update.end()) {
        udpMulticast.printf("ERR UPDATE:%s Abort finish failed: %u",
                            SHI::config.name, Update.getError());
      } else {
        udpMulticast.printf("OK UPDATE:%s Finished", SHI::config.name);
      }
    } else {
      udpMulticast.printf("ERR UPDATE:%s Abort, written:%d size:%d",
                          SHI::config.name, written, size);
    }
    SHI::config.canary = 0xDEADBEEF;
    configPrefs.putBytes(CONFIG, &SHI::config, sizeof(SHI::config_t));
    SHI::hw.resetWithReason("Firmware updated");
  }
  http.end();
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
  if (doUpdate) {
    if (isUpdateAvailable()) {
      startUpdate();
    } else {
      logInfo("ESP32", __func__, "No newer version available");
      udpMulticast.printf("OK UPDATE:%s No update available", SHI::config.name);
    }
    doUpdate = false;
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
  return true;
}

void SHI::HWBase::feedWatchdog() {
  timerWrite(timer, 0); // reset timer (feed watchdog)
}
