#include "WifiBase.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <map>
#include <rom/rtc.h>

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

const char *ssid = "Elfenburg";
const char *password = "fe-shnyed-olv-ek";

AsyncUDP udpMulticast;
const int CONNECT_TIMEOUT = 500;
const int DATA_TIMEOUT = 1000;
const String STATUS_ITEM = "Status";
int errorCount = 0, httpErrorCount = 0, httpCount = 0;

#ifndef VER_MAJ
#error "Major version undefined"
#endif
#ifndef VER_MIN
#error "Minor version undefined"
#endif
#ifndef VER_PAT
#error "Patch version undefined"
#endif
const uint8_t MAJOR_VERSION = VER_MAJ;
const uint8_t MINOR_VERSION = VER_MIN;
const uint8_t PATCH_VERSION = VER_PAT;
const String VERSION = String(MAJOR_VERSION, 10) + "." +
                       String(MINOR_VERSION, 10) + "." +
                       String(PATCH_VERSION, 10);

const char *CONFIG = "wifiConfig";
const uint32_t CONST_MARKER = 0xAFFEDEAD;
config_t config;
Preferences configPrefs;
bool doUpdate = false;

const int wdtTimeout = 15000; // time in ms to trigger the watchdog
hw_timer_t *timer = NULL;

void IRAM_ATTR resetModule() {
  ets_printf("Watchdog bit, reboot\n");
  resetWithReason("Watchdog triggered", false);
  esp_restart();
}

void errLeds(void) {
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

void printError(HTTPClient &http, int httpCode) {
  // httpCode will be negative on error
  if (httpCode > 0) {
    if (httpCode < 200 || httpCode > 299)
      httpErrorCount++;
    httpCount++;
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] PUT... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    errorCount++;
    Serial.printf("[HTTP] PUT... failed, error: %s\n",
                  http.errorToString(httpCode).c_str());
  }
}

void uploadInfo(String name, String item, float value) {
  uploadInfo(name, item, String(value, 1));
}

void uploadInfo(String name, String item, String value) {
  Serial.print(name + " " + item + " " + value);
  bool tryHard = false;
  if (item == STATUS_ITEM && value != "OK") {
    tryHard = true;
  }
  int retryCount = 0;
  do {
    HTTPClient http;
    http.begin("http://192.168.188.202:8080/rest/items/esp32" + name + item +
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

void uploadInfo(String item, String value) {
  uploadInfo(String(config.name), item, value);
}

void uploadInfo(String item, float value) {
  uploadInfo(String(config.name), item, String(value, 1));
}

void setupWatchdog() {
  timer = timerBegin(0, 80, true);                  // timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  // attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); // set time in us
  timerAlarmEnable(timer);
}

void disableWatchdog() { timerEnd(timer); }

void resetWithReason(String reason, bool restart = true) {
  reason.toCharArray(config.resetReason, sizeof(config.resetReason));
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
  if (restart) {
    Serial.println("Restarting:" + reason);
    delay(100);
    ESP.restart();
  }
}

void printConfig() {
  Serial.printf("IP address:  %08X\n", config.local_IP);
  Serial.printf("Subnet Mask: %08X\n", config.subnet);
  Serial.printf("Gateway IP:  %08X\n", config.gateway);
  Serial.printf("Canary:      %08X\n", config.canary);
  Serial.printf("Name:        %-40s\n", config.name);
  Serial.printf("Reset reason:%-40s\n", config.resetReason);
}

bool getHostName() {
  HTTPClient http;
  String mac = WiFi.macAddress();
  mac.replace(':', '_');
  http.begin("http://192.168.188.202/esp/" + mac);
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String newName = http.getString();
    newName.replace('\n', '\0');
    newName.trim();
    if (newName.length() == 0)
      return false;
    newName.toCharArray(config.name, sizeof(config.name));
    Serial.printf("Recevied new Name:%s : %s\n", newName.c_str(), config.name);
    return true;
  }
  return false;
}

void updateHandler(AsyncUDPPacket packet) {
  Serial.println("UPDATE called");
  packet.printf("OK UPDATE:%s", config.name);
  doUpdate = true;
}

void resetHandler(AsyncUDPPacket packet) {
  Serial.println("RESET called");
  packet.printf("OK RESET:%s", config.name);
  packet.flush();
  resetWithReason("UDP RESET request");
}

void reconfHandler(AsyncUDPPacket packet) {
  Serial.println("RECONF called");
  config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
  packet.printf("OK RECONF:%s", config.name);
  packet.flush();
  resetWithReason("UDP RECONF request");
}

void infoHandler(AsyncUDPPacket packet) {
  Serial.println("INFO called");
  packet.printf("OK INFO:%s\n%s\n%s\n%lu\n%s:%s\n%s\n%s\n%d\n%d\n%d\n",
                config.name, VERSION.c_str(), config.resetReason, millis(),
                RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
                RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
                WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(),
                httpCount, errorCount, httpErrorCount);
}

void versionHandler(AsyncUDPPacket packet) {
  Serial.println("VERSION called");
  packet.printf("OK VERSION:%s %s", config.name, VERSION.c_str());
}

std::map<String, AuPacketHandlerFunction> registeredHandlers = {
    {"UPDATE", updateHandler},
    {"RECONF", reconfHandler},
    {"RESET", resetHandler},
    {"INFO", infoHandler},
    {"VERSION", versionHandler}};

void addUDPPacketHandler(String trigger, AuPacketHandlerFunction handler) {
  registeredHandlers[trigger] = handler;
}

void handleUDPPacket(AsyncUDPPacket packet) {
  const char *data = (const char *)(packet.data());
  if (packet.length() < 10) {
    auto handler = registeredHandlers[String(data)];
    if (handler != NULL) {
      handler(packet);
    }
  }
}

void wifiDoSetup(String defaultName) {
  IPAddress primaryDNS(192, 168, 188, 202); // optional
  IPAddress secondaryDNS(192, 168, 188, 1); // optional
  setupWatchdog();
  feedWatchdog();

  Serial.begin(115200);

  configPrefs.begin(CONFIG);
  configPrefs.getBytes(CONFIG, &config, sizeof(config_t));
  if (config.canary == CONST_MARKER) {
    Serial.println("Restoring config from memory");
    printConfig();
    WiFi.setHostname(config.name);
    if (!WiFi.config(config.local_IP, config.gateway, config.subnet, primaryDNS,
                     secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }
  } else {
    Serial.printf("Canary mismatch, stored: %08X\n", config.canary);
    defaultName.toCharArray(config.name, sizeof(config.name));
  }

  Serial.print("Connecting to " + String(ssid));

  feedWatchdog();
  WiFi.begin(ssid, password);

  int connectCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (connectCount > 5) {
      resetWithReason("Initial wifi connection attempts exceeded");
    }
    delay(500);
    Serial.print(".");
    connectCount++;
  }
  feedWatchdog();
  Serial.println("WiFi connected!");
  if (config.canary != CONST_MARKER && getHostName()) {
    Serial.println("Storing config");
    config.local_IP = WiFi.localIP();
    config.gateway = WiFi.gatewayIP();
    config.subnet = WiFi.subnetMask();
    // config.name is set by getHostName
    WiFi.setHostname(config.name);
    resetWithReason("Fresh-reset", false);
    config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    Serial.println("ESP Mac Address: " + WiFi.macAddress());
    printConfig();
  }
  uploadInfo(config.name, STATUS_ITEM,
             "STARTED: " + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
                 RESET_SOURCE[rtc_get_reset_reason(1)] + " " +
                 String(config.resetReason));
  feedWatchdog();
  if (udpMulticast.listenMulticast(IPAddress(239, 1, 23, 42), 2323)) {
    udpMulticast.onPacket(handleUDPPacket);
  }
}

void updateProgress(size_t a, size_t b) {
  udpMulticast.printf("OK UPDATE:%s %u/%u", config.name, a, b);
  feedWatchdog();
}

bool isUpdateAvailable() {
  HTTPClient http;
  http.begin("http://192.168.188.202/esp/firmware/" + String(config.name) +
             ".version");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String remoteVersion = http.getString();
    Serial.println("Remote version is:" + remoteVersion);
    return VERSION.compareTo(remoteVersion) < 0;
  }
  return false;
}

void startUpdate() {
  HTTPClient http;
  http.begin("http://192.168.188.202/esp/firmware/" + String(config.name) +
             ".bin");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    udpMulticast.printf("OK UPDATE:%s Starting", config.name);
    int size = http.getSize();
    if (size < 0) {
      udpMulticast.printf("ERR UPDATE:%s Abort, no size", config.name);
      return;
    }
    if (!Update.begin(size)) {
      udpMulticast.printf("ERR UPDATE:%s Abort, not enough space", config.name);
      return;
    }
    Update.onProgress(updateProgress);
    size_t written = Update.writeStream(http.getStream());
    if (written == size) {
      udpMulticast.printf("OK UPDATE:%s Finishing", config.name);
      if (!Update.end()) {
        udpMulticast.printf("ERR UPDATE:%s Abort finish failed: %u",
                            config.name, Update.getError());
      } else {
        udpMulticast.printf("OK UPDATE:%s Finished", config.name);
      }
    } else {
      udpMulticast.printf("ERR UPDATE:%s Abort, written:%d size:%d",
                          config.name, written, size);
    }
    config.canary = 0xDEADBEEF;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    resetWithReason("Firmware updated");
  }
  http.end();
}

bool wifiIsConntected() {
  static int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (retryCount > 6) {
      resetWithReason("Retry count for Wifi exceeded");
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    delay(500);
    retryCount++;
  }
  retryCount = 0;
  if (doUpdate) {
    if (isUpdateAvailable()) {
      startUpdate();
    } else {
      Serial.println("No newer version available");
      udpMulticast.printf("OK UPDATE:%s No update available", config.name);
    }
    doUpdate = false;
  }
  return true;
}

void feedWatchdog() {
  timerWrite(timer, 0); // reset timer (feed watchdog)
}
