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
HardwareSerial *debugSerial=&Serial;
#else
NullPrint null_stream;
NullPrint *debugSerial=&null_stream;
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

#ifdef HAS_DISPLAY
  SSD1306Wire display = SSD1306Wire(0x3c, SDA_OLED, SCL_OLED, RST_OLED, GEOMETRY_128_64);
  static String displayLineBuf[7]={"Temperatur:","n/a","Luftfeuchtigkeit:","n/a","Luftqualität:","n/a",""};
  bool displayUpdated=false;
#endif

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
    debugSerial->printf("[HTTP] PUT... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      debugSerial->println(payload);
    }
  } else {
    errorCount++;
    debugSerial->printf("[HTTP] PUT... failed, error: %s\n",
                  http.errorToString(httpCode).c_str());
  }
}
#ifdef HAS_DISPLAY
void setBrightness(uint8_t value){
  display.setBrightness(value);
}
#endif

void uploadInfo(String name, String item, float value) {
  uploadInfo(name, item, String(value, 1));
}

void uploadInfo(String name, String item, String value) {
  debugSerial->print(name + " " + item + " " + value);
  bool tryHard = false;
  if (item == STATUS_ITEM && value != "OK") {
    tryHard = true;
  }
  #ifdef HAS_DISPLAY
  if (item == STATUS_ITEM) {
    displayLineBuf[6]=value;
    displayUpdated=true;
  } else if (item == "Temperature") {
    displayLineBuf[1]=value+"°C";
    displayUpdated=true;
  } else if (item == "Humidity") {
    displayLineBuf[3]=value+"%";
    displayUpdated=true;
  } if (item == "StaticIaq") {
    displayLineBuf[5]=value;
    displayUpdated=true;
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
    debugSerial->println("Restarting:" + reason);
    delay(100);
    ESP.restart();
  }
}

void printConfig() {
  debugSerial->printf("IP address:  %08X\n", config.local_IP);
  debugSerial->printf("Subnet Mask: %08X\n", config.subnet);
  debugSerial->printf("Gateway IP:  %08X\n", config.gateway);
  debugSerial->printf("Canary:      %08X\n", config.canary);
  debugSerial->printf("Name:        %-40s\n", config.name);
  debugSerial->printf("Reset reason:%-40s\n", config.resetReason);
}

bool getHostName() {
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
    newName.toCharArray(config.name, sizeof(config.name));
    debugSerial->printf("Recevied new Name:%s : %s\n", newName.c_str(), config.name);
    return true;
  }
  return false;
}

void updateHandler(AsyncUDPPacket &packet) {
  debugSerial->println("UPDATE called");
  packet.printf("OK UPDATE:%s", config.name);
  doUpdate = true;
}

void resetHandler(AsyncUDPPacket &packet) {
  debugSerial->println("RESET called");
  packet.printf("OK RESET:%s", config.name);
  packet.flush();
  resetWithReason("UDP RESET request");
}

void reconfHandler(AsyncUDPPacket &packet) {
  debugSerial->println("RECONF called");
  config.canary = 0xDEADBEEF;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
  packet.printf("OK RECONF:%s", config.name);
  packet.flush();
  resetWithReason("UDP RECONF request");
}

void infoHandler(AsyncUDPPacket &packet) {
  debugSerial->println("INFO called");
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
                config.name, VERSION.c_str(), config.resetReason, millis(),
                RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
                RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
                WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(),
                httpCount, errorCount, httpErrorCount);
}

void versionHandler(AsyncUDPPacket &packet) {
  debugSerial->println("VERSION called");
  packet.printf("OK VERSION:%s\nVersion:%s", config.name, VERSION.c_str());
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

void handleUDPPacket(AsyncUDPPacket &packet) {
  const char *data = (const char *)(packet.data());
  if (packet.length() < 10) {
    auto handler = registeredHandlers[String(data)];
    if (handler != NULL) {
      handler(packet);
    }
  }
}

String getConfigName() { return String(config.name); }

void wifiDoSetup(String defaultName) {
  IPAddress primaryDNS(192, 168, 188, 202); // optional
  IPAddress secondaryDNS(192, 168, 188, 1); // optional
  setupWatchdog();
  feedWatchdog();
  debugSerial->begin(115200);
  #ifdef HAS_DISPLAY
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "OLED initial done!");
    display.display();
  #endif

  configPrefs.begin(CONFIG);
  configPrefs.getBytes(CONFIG, &config, sizeof(config_t));
  if (config.canary == CONST_MARKER) {
    debugSerial->println("Restoring config from memory");
    printConfig();
    WiFi.setHostname(config.name);
    if (!WiFi.config(config.local_IP, config.gateway, config.subnet, primaryDNS,
                     secondaryDNS)) {
      debugSerial->println("STA Failed to configure");
    }
  } else {
    debugSerial->printf("Canary mismatch, stored: %08X\n", config.canary);
    defaultName.toCharArray(config.name, sizeof(config.name));
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
  if (config.canary != CONST_MARKER && getHostName()) {
    debugSerial->println("Storing config");
    config.local_IP = WiFi.localIP();
    config.gateway = WiFi.gatewayIP();
    config.subnet = WiFi.subnetMask();
    // config.name is set by getHostName
    WiFi.setHostname(config.name);
    resetWithReason("Fresh-reset", false);
    config.canary = CONST_MARKER;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    debugSerial->println("ESP Mac Address: " + WiFi.macAddress());
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
  http.begin("http://192.168.188.250/esp/firmware/" + String(config.name) +
             ".version");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String remoteVersion = http.getString();
    debugSerial->println("Remote version is:" + remoteVersion);
    return VERSION.compareTo(remoteVersion) < 0;
  }
  return false;
}

void startUpdate() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" + String(config.name) +
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
    feedWatchdog();
    if (retryCount > 6) {
      resetWithReason("Retry count for Wifi exceeded"+WiFi.status());
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    retryCount++;
    delay(retryCount*1000);
  }
  retryCount = 0;
  if (doUpdate) {
    if (isUpdateAvailable()) {
      startUpdate();
    } else {
      debugSerial->println("No newer version available");
      udpMulticast.printf("OK UPDATE:%s No update available", config.name);
    }
    doUpdate = false;
  }
  #ifdef HAS_DISPLAY
    if (displayUpdated) {
      display.clear();
      for (int i=1;i<6;i+=2)
        display.drawString(90,(i/2)*13, displayLineBuf[i]);
      for (int i=0;i<6;i+=2)
        display.drawString(0,(i/2)*13, displayLineBuf[i]);
      display.drawStringMaxWidth(0,3*13, 128, displayLineBuf[6]);
      display.display();
      displayUpdated=false;
    }
  #endif
  return true;
}

void feedWatchdog() {
  timerWrite(timer, 0); // reset timer (feed watchdog)
}
