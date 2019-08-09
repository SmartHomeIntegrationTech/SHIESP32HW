#include "WifiBase.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <CRC32.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
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

AsyncUDP udp;

const char *CONFIG = "wifiConfig";
config_t config;
Preferences configPrefs;

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
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] PUT... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] PUT... failed, error: %s\n",
                  http.errorToString(httpCode).c_str());
  }
}

void uploadInfo(String name, String item, float value) {
  uploadInfo(name, item, String(value, 1));
}

void uploadInfo(String name, String item, String value) {
  Serial.print(name + " " + item + " " + value);
  HTTPClient http;
  http.begin("http://192.168.188.202:8080/rest/items/esp32" + name + item +
             "/state");
  http.setConnectTimeout(100);
  http.setTimeout(1000);
  int httpCode = http.PUT(value);
  printError(http, httpCode);
  http.end();
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

void resetWithReason(String reason, bool restart = true) {
  const size_t reasonSize = sizeof(config.resetReason) - 1;
  strncpy(config.resetReason, reason.c_str(), reasonSize);
  config.resetReason[reasonSize] = 0;
  configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
  if (restart) {
    Serial.println("Restarting:" + reason);
    delay(100);
    ESP.restart();
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
  auto chkSum = CRC32::calculate(&config, sizeof(config_t));
  if (config.canary == chkSum) {
    WiFi.setHostname(config.name);
    if (!WiFi.config(config.local_IP, config.gateway, config.subnet, primaryDNS,
                     secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }
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
  const auto configNameSize = sizeof(config.name) - 1;
  strncpy(config.name, defaultName.c_str(), configNameSize);
  config.name[configNameSize] = 0;
  if (config.canary != chkSum) {
    Serial.println("Storing config");
    config.local_IP = WiFi.localIP();
    config.gateway = WiFi.gatewayIP();
    config.subnet = WiFi.subnetMask();
    // config.name=WiFi.getHostname(); Doesn't work due to a Bug :(
    resetWithReason("Fresh-reset", false);
    config.canary = CRC32::calculate(&config, sizeof(config_t));
    ;
    configPrefs.putBytes(CONFIG, &config, sizeof(config_t));
    Serial.println("IP address: " + config.local_IP);
    Serial.println("ESP Mac Address: " + WiFi.macAddress());
    Serial.println("Subnet Mask: " + config.subnet);
    Serial.println("Gateway IP: " + config.gateway);
    Serial.println("Name: " + String(config.name));
  } else {
    Serial.println("Config read:" + String(config.name));
  }
  uploadInfo(config.name, "status",
             "STARTED: " + RESET_SOURCE[rtc_get_reset_reason(0)] + ":" +
                 RESET_SOURCE[rtc_get_reset_reason(1)] + " " +
                 String(config.resetReason));
  feedWatchdog();
  if (udp.listen(2323)) {
    udp.onPacket([](AsyncUDPPacket packet) {
      const char *data = (const char *)(packet.data());
      if (strncmp("UPDATE", data, 6) == 0) {
      }
      if (strncmp("RESET", data, 5) == 0) {
        resetWithReason("UDP reset request");
      }
    });
  }
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
    delay(100);
    retryCount++;
  }
  retryCount = 0;
  return true;
}

void feedWatchdog() {
  timerWrite(timer, 0); // reset timer (feed watchdog)
}
