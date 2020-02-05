#include "SHIMulticastHandler.h"
#include "SHIHardware.h"
#include "WifiBase.h"
#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Update.h>
#include <rom/rtc.h>

namespace {

const int CONNECT_TIMEOUT = 500;
const int DATA_TIMEOUT = 1000;
PROGMEM const String RESET_SOURCE[] = {
    "NO_MEAN",          "POWERON_RESET",    "SW_RESET",
    "OWDT_RESET",       "DEEPSLEEP_RESET",  "SDIO_RESET",
    "TG0WDT_SYS_RESET", "TG1WDT_SYS_RESET", "RTCWDT_SYS_RESET",
    "INTRUSION_RESET",  "TGWDT_CPU_RESET",  "SW_CPU_RESET",
    "RTCWDT_CPU_RESET", "EXT_CPU_RESET",    "RTCWDT_BROWN_OUT_RESET",
    "RTCWDT_RTC_RESET"};

} // namespace

void SHI::SHIMulticastHandler::updateProgress(size_t a, size_t b) {
  udpMulticast.printf("OK UPDATE:%s %u/%u", SHI::hw.getNodeName().c_str(), a,
                      b);
  SHI::hw.feedWatchdog();
}

bool SHI::SHIMulticastHandler::isUpdateAvailable() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" + SHI::hw.getNodeName() +
             ".version");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String remoteVersion = http.getString();
    SHI::hw.logInfo(name, __func__, "Remote version is:" + remoteVersion);
    return SHI::VERSION.compareTo(remoteVersion) < 0;
  }
  return false;
}

void SHI::SHIMulticastHandler::startUpdate() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" + SHI::hw.getNodeName() +
             ".bin");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    udpMulticast.printf("OK UPDATE:%s Starting", SHI::hw.getNodeName().c_str());
    int size = http.getSize();
    if (size < 0) {
      udpMulticast.printf("ERR UPDATE:%s Abort, no size",
                          SHI::hw.getNodeName().c_str());
      return;
    }
    if (!Update.begin(size)) {
      udpMulticast.printf("ERR UPDATE:%s Abort, not enough space",
                          SHI::hw.getNodeName().c_str());
      return;
    }
    auto progress = std::bind(&SHI::SHIMulticastHandler::updateProgress, this,
                              std::placeholders::_1, std::placeholders::_2);
    Update.onProgress(progress);
    size_t written = Update.writeStream(http.getStream());
    if (written == size) {
      udpMulticast.printf("OK UPDATE:%s Finishing",
                          SHI::hw.getNodeName().c_str());
      if (!Update.end()) {
        udpMulticast.printf("ERR UPDATE:%s Abort finish failed: %u",
                            SHI::hw.getNodeName().c_str(), Update.getError());
      } else {
        udpMulticast.printf("OK UPDATE:%s Finished",
                            SHI::hw.getNodeName().c_str());
      }
    } else {
      udpMulticast.printf("ERR UPDATE:%s Abort, written:%d size:%d",
                          SHI::hw.getNodeName().c_str(), written, size);
    }
    SHI::hw.resetConfig();
    SHI::hw.resetWithReason("Firmware updated", true);
  }
  http.end();
}

void SHI::SHIMulticastHandler::loopCommunication() {
  if (doUpdate) {
    if (isUpdateAvailable()) {
      startUpdate();
    } else {
      SHI::hw.logInfo(name, __func__, "No newer version available");
      udpMulticast.printf("OK UPDATE:%s No update available",
                          SHI::hw.getNodeName().c_str());
    }
    doUpdate = false;
  }
}

void SHI::SHIMulticastHandler::setupCommunication() {
  if (udpMulticast.listenMulticast(IPAddress(239, 1, 23, 42), 2323)) {
    auto handleUDP = std::bind(&SHI::SHIMulticastHandler::handleUDPPacket, this,
                               std::placeholders::_1);
    udpMulticast.onPacket(handleUDP);
  }
}

void SHI::SHIMulticastHandler::updateHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo(name, __func__, "UPDATE called");
  packet.printf("OK UPDATE:%s", SHI::hw.getNodeName().c_str());
  doUpdate = true;
}

void SHI::SHIMulticastHandler::resetHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo(name, __func__, "RESET called");
  packet.printf("OK RESET:%s", SHI::hw.getNodeName().c_str());
  packet.flush();
  SHI::hw.resetWithReason("UDP RESET request", true);
}

void SHI::SHIMulticastHandler::reconfHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo(name, __func__, "RECONF called");
  SHI::hw.resetConfig();
  packet.printf("OK RECONF:%s", SHI::hw.getNodeName().c_str());
  packet.flush();
  SHI::hw.resetWithReason("UDP RECONF request", true);
}

void SHI::SHIMulticastHandler::infoHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo(name, __func__, "INFO called");
  packet.printf("OK INFO:%s\n"
                "Version:%s\n"
                "ResetReason:%s\n"
                "RunTimeInMillis:%lu\n"
                "ResetSource:%s:%s\n"
                "LocalIP:%s\n"
                "Mac:%s\n",
                SHI::hw.getNodeName().c_str(), SHI::VERSION.c_str(),
                SHI::config.resetReason, millis(),
                RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
                RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
                WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
}

void SHI::SHIMulticastHandler::versionHandler(AsyncUDPPacket &packet) {
  SHI::hw.logInfo(name, __func__, "VERSION called");
  packet.printf("OK VERSION:%s\nVersion:%s", SHI::hw.getNodeName().c_str(),
                SHI::VERSION.c_str());
}

void SHI::SHIMulticastHandler::addUDPPacketHandler(
    String trigger, AuPacketHandlerFunction handler) {
  registeredHandlers[trigger] = handler;
}

void SHI::SHIMulticastHandler::handleUDPPacket(AsyncUDPPacket &packet) {
  const char *data = (const char *)(packet.data());
  if (packet.length() < 10) {
    auto handler = registeredHandlers[String(data)];
    if (handler != NULL) {
      handler(packet);
    }
  }
}
