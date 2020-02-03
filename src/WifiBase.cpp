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
const int SHI::CONNECT_TIMEOUT = 500;
const int SHI::DATA_TIMEOUT = 1000;

int errorCount = 0, httpErrorCount = 0, httpCount = 0;

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

void SHI::HWBase::addChannel(std::shared_ptr<SHI::Channel> channel) {
  channels.push_back(channel);
}

void SHI::HWBase::loop() {
  for (auto &&channel : channels) {
    auto sensor = channel->sensor;
    auto result = sensor->readSensor();
    auto sensorName = sensor->getName();
    auto channelName = channel->name;
    feedWatchdog();
    if (result == nullptr) {
      auto msg = sensor->getStatusMessage();
      uploadInfo(getConfigName(), STATUS_ITEM, msg);
      feedWatchdog();
      if (sensor->errorIsFatal()) {
        while (true) {
          errLeds();
        }
      }
    } else {
      for (auto &&data : result->data) {
        if (data->type != SHI::NO_DATA) {
          uploadInfo(getConfigName(), channelName + sensorName + data->name,
                     data->toTransmitString(*data));
          feedWatchdog();
        }
      }
      uploadInfo(getConfigName(), STATUS_ITEM, STATUS_OK);
    }
  }
}

void printError(HTTPClient &http, int httpCode) {
  // httpCode will be negative on error
  if (httpCode > 0) {
    if (httpCode < 200 || httpCode > 299)
      httpErrorCount++;
    httpCount++;
    // HTTP header has been send and Server response header has been handled
    SHI::hw.debugSerial->printf("[HTTP] PUT... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      SHI::hw.debugSerial->println(payload);
    }
  } else {
    errorCount++;
    SHI::hw.debugSerial->printf("[HTTP] PUT... failed, error: %s\n",
                                http.errorToString(httpCode).c_str());
  }
}

void SHI::HWBase::setDisplayBrightness(uint8_t value) {
#ifdef HAS_DISPLAY
  display.setBrightness(value);
#endif
}

void SHI::HWBase::uploadInfo(String name, String item, String value) {
  debugSerial->print(name + " " + item + " " + value);
  bool tryHard = false;
  if (item == STATUS_ITEM && value != STATUS_OK) {
    tryHard = true;
  }
#ifdef HAS_DISPLAY
  if (item == STATUS_ITEM) {
    displayLineBuf[6] = value;
    displayUpdated = true;
  } else if (item == "Temperature") {
    displayLineBuf[1] = value + "°C";
    displayUpdated = true;
  } else if (item == "Humidity") {
    displayLineBuf[3] = value + "%";
    displayUpdated = true;
  } else if (item == "StaticIaq") {
    displayLineBuf[5] = value;
    displayUpdated = true;
  } else if (item == "PM10") {
    displayLineBuf[2] = "PM10";
    displayLineBuf[3] = value + "µg";
    displayUpdated = true;
  } else if (item == "PM2_5") {
    displayLineBuf[0] = "PM2.5";
    displayLineBuf[1] = value + "µg";
    displayUpdated = true;
  }
#endif
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
    if (httpCode == 202)
      return; // Either return early or try until success
    retryCount++;
    feedWatchdog();
  } while (tryHard && retryCount < 15);
}

void SHI::HWBase::setupWatchdog() {
  timer = timerBegin(0, 80, true);                  // timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  // attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); // set time in us
  timerAlarmEnable(timer);
}

void disableWatchdog() { timerEnd(timer); }

void SHI::HWBase::resetWithReason(String reason, bool restart = true) {
  reason.toCharArray(SHI::config.resetReason, sizeof(SHI::config.resetReason));
  configPrefs.putBytes(CONFIG, &config, sizeof(SHI::config_t));
  if (restart) {
    debugSerial->println("Restarting:" + reason);
    delay(100);
    ESP.restart();
  }
}

void printConfig() {
  SHI::hw.debugSerial->printf("IP address:  %08X\n", SHI::config.local_IP);
  SHI::hw.debugSerial->printf("Subnet Mask: %08X\n", SHI::config.subnet);
  SHI::hw.debugSerial->printf("Gateway IP:  %08X\n", SHI::config.gateway);
  SHI::hw.debugSerial->printf("Canary:      %08X\n", SHI::config.canary);
  SHI::hw.debugSerial->printf("Name:        %-40s\n", SHI::config.name);
  SHI::hw.debugSerial->printf("Reset reason:%-40s\n", SHI::config.resetReason);
}

bool updateHostName() {
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
    SHI::hw.debugSerial->printf("Recevied new Name:%s : %s\n", newName.c_str(),
                                SHI::config.name);
    return true;
  } else {
    SHI::hw.debugSerial->println("Failed to update name for mac:" + mac);
  }
  return false;
}

void updateHandler(AsyncUDPPacket &packet) {
  SHI::hw.debugSerial->println("UPDATE called");
  packet.printf("OK UPDATE:%s", SHI::config.name);
  doUpdate = true;
}

void resetHandler(AsyncUDPPacket &packet) {
  SHI::hw.debugSerial->println("RESET called");
  packet.printf("OK RESET:%s", SHI::config.name);
  packet.flush();
  SHI::hw.resetWithReason("UDP RESET request");
}

void reconfHandler(AsyncUDPPacket &packet) {
  SHI::hw.debugSerial->println("RECONF called");
  SHI::config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &SHI::config, sizeof(SHI::config_t));
  packet.printf("OK RECONF:%s", SHI::config.name);
  packet.flush();
  SHI::hw.resetWithReason("UDP RECONF request");
}

void infoHandler(AsyncUDPPacket &packet) {
  SHI::hw.debugSerial->println("INFO called");
  packet.printf("OK INFO:%s\n"
                "Version:%s\n"
                "ResetReason:%s\n"
                "RunTimeInMillis:%lu\n"
                "ResetSource:%s:%s\n"
                "LocalIP:%s\n"
                "Mac:%s\n"
                "HttpCount:%d\n"
                "ErrorCount:%d\n"
                "HttpErrorCount:%d\n",
                SHI::config.name, SHI::VERSION.c_str(), SHI::config.resetReason,
                millis(), RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
                RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
                WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(),
                httpCount, errorCount, httpErrorCount);
}

void versionHandler(AsyncUDPPacket &packet) {
  SHI::hw.debugSerial->println("VERSION called");
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

String SHI::HWBase::getConfigName() { return String(SHI::config.name); }

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
    debugSerial->println("Restoring config from memory");
    printConfig();
    WiFi.setHostname(SHI::config.name);
    if (!WiFi.config(SHI::config.local_IP, SHI::config.gateway,
                     SHI::config.subnet, primaryDNS, secondaryDNS)) {
      debugSerial->println("STA Failed to configure");
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
  debugSerial->println("WiFi connected!");
  if (SHI::config.canary != CONST_MARKER && updateHostName()) {
    debugSerial->println("Storing config");
    SHI::config.local_IP = WiFi.localIP();
    SHI::config.gateway = WiFi.gatewayIP();
    SHI::config.subnet = WiFi.subnetMask();
    // SHI::config.name is set by updateHostName
    WiFi.setHostname(SHI::config.name);
    resetWithReason("Fresh-reset", false);
    SHI::config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    debugSerial->println("ESP Mac Address: " + WiFi.macAddress());
    printConfig();
  }
  uploadInfo(SHI::config.name, STATUS_ITEM,
             "STARTED: " + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
                 RESET_SOURCE[rtc_get_reset_reason(1)] + " " +
                 String(SHI::config.resetReason));
  feedWatchdog();
  if (udpMulticast.listenMulticast(IPAddress(239, 1, 23, 42), 2323)) {
    udpMulticast.onPacket(handleUDPPacket);
  }

  for (auto &&channel : channels) {
    auto sensor = channel->sensor;
    String name = sensor->getName();
    debugSerial->println("Setting up: " + name);
    if (!sensor->setupSensor()) {
      debugSerial->println(
          "Something went wrong when setting up sensor:" + name +
          channel->name + " " + sensor->getStatusMessage());
      while (1) {
        errLeds();
      }
    }
    feedWatchdog();
    debugSerial->println("Setup done of: " + name);
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
    SHI::hw.debugSerial->println("Remote version is:" + remoteVersion);
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
      debugSerial->println("No newer version available");
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
