/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIMulticastHandler.h"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Update.h>
#include <rom/rtc.h>

#include "SHIHardware.h"

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

class StatsVisitor : public SHI::Visitor {
 public:
  void visit(SHI::Sensor *sensor) {
    for (auto &&stat : sensor->getStatistics()) {
      statString += String(sensor->getName()) + "." + stat.first + ":" +
                    stat.second + "\n";
    }
  }
  void visit(SHI::Hardware *hardware) {
    for (auto &&stat : hardware->getStatistics()) {
      statString += String(hardware->getName()) + "." + stat.first + ":" +
                    stat.second + "\n";
    }
  }
  void visit(SHI::Communicator *communicator) {
    for (auto &&stat : communicator->getStatistics()) {
      statString += String(communicator->getName()) + "." + stat.first + ":" +
                    stat.second + "\n";
    }
  }
  String statString = "";
};
}  // namespace

void SHI::MulticastHandler::updateProgress(size_t a, size_t b) {
  udpMulticast.printf("OK UPDATE:%s %u/%u\n", SHI::hw->getNodeName(), a, b);
  SHI::hw->feedWatchdog();
}

bool SHI::MulticastHandler::isUpdateAvailable() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" +
             String(SHI::hw->getNodeName()) + ".version");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String remoteVersion = http.getString();
    SHI_LOGINFO(("Remote version is:" + remoteVersion).c_str());
    return String(SHI::VERSION).compareTo(remoteVersion) < 0;
  }
  return false;
}

void SHI::MulticastHandler::startUpdate() {
  HTTPClient http;
  http.begin("http://192.168.188.250/esp/firmware/" +
             String(SHI::hw->getNodeName()) + ".bin");
  http.setConnectTimeout(CONNECT_TIMEOUT);
  http.setTimeout(DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode == 200) {
    udpMulticast.printf("OK UPDATE:%s Starting\n", SHI::hw->getNodeName());
    int size = http.getSize();
    if (size < 0) {
      udpMulticast.printf("ERR UPDATE:%s Abort, no size\n",
                          SHI::hw->getNodeName());
      return;
    }
    if (!Update.begin(size)) {
      udpMulticast.printf("ERR UPDATE:%s Abort, not enough space\n",
                          SHI::hw->getNodeName());
      return;
    }
    Update.onProgress([this](size_t a, size_t b) { updateProgress(a, b); });
    size_t written = Update.writeStream(http.getStream());
    if (written == size) {
      udpMulticast.printf("OK UPDATE:%s Finishing\n", SHI::hw->getNodeName());
      if (!Update.end()) {
        udpMulticast.printf("ERR UPDATE:%s Abort finish failed: %u\n",
                            SHI::hw->getNodeName(), Update.getError());
      } else {
        udpMulticast.printf("OK UPDATE:%s Finished\n", SHI::hw->getNodeName());
      }
    } else {
      udpMulticast.printf("ERR UPDATE:%s Abort, written:%d size:%d\n",
                          SHI::hw->getNodeName(), written, size);
    }
    SHI::hw->resetConfig();
    SHI::hw->resetWithReason("Firmware updated", true);
  }
  http.end();
}

void SHI::MulticastHandler::loopCommunication() {
  if (doUpdate) {
    if (isUpdateAvailable()) {
      startUpdate();
    } else {
      SHI_LOGINFO("No newer version available");
      udpMulticast.printf("OK UPDATE:%s No update available\n",
                          SHI::hw->getNodeName());
    }
    doUpdate = false;
  }
}

void SHI::MulticastHandler::setupCommunication() {
  if (udpMulticast.listenMulticast(IPAddress(239, 1, 23, 42), 2323)) {
    auto handleUDP = std::bind(&SHI::MulticastHandler::handleUDPPacket, this,
                               std::placeholders::_1);
    udpMulticast.onPacket(handleUDP);
  }
}

void SHI::MulticastHandler::updateHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("UPDATE called");
  packet->printf("OK UPDATE:%s", SHI::hw->getNodeName());
  packet->flush();
  doUpdate = true;
}

void SHI::MulticastHandler::resetHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("RESET called");
  packet->printf("OK RESET:%s", SHI::hw->getNodeName());
  packet->flush();
  SHI::hw->resetWithReason("UDP RESET request", true);
}

void SHI::MulticastHandler::reconfHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("RECONF called");
  SHI::hw->resetConfig();
  packet->printf("OK RECONF:%s", SHI::hw->getNodeName());
  packet->flush();
  SHI::hw->resetWithReason("UDP RECONF request", true);
}

void SHI::MulticastHandler::infoHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("INFO called");
  StatsVisitor stats;
  SHI::hw->accept(stats);
  SHI_LOGINFO(("Stats:" + stats.statString).c_str());
  packet->printf(
      "OK INFO:%s\n"
      "Version:%s\n"
      "ResetReason:%s\n"
      "RunTimeInMillis:%lu\n"
      "ResetSource:%s:%s\n"
      "LocalIP:%s\n"
      "Mac:%s\n"
      "%s",
      SHI::hw->getNodeName(), SHI::VERSION, SHI::hw->getResetReason(), millis(),
      RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
      RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
      WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(),
      stats.statString.c_str());
  packet->flush();
}

void SHI::MulticastHandler::versionHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("VERSION called");
  packet->printf("OK VERSION:%s\nVersion:%s", SHI::hw->getNodeName(),
                 SHI::VERSION);
}

void SHI::MulticastHandler::addUDPPacketHandler(
    String trigger, AuPacketHandlerFunction handler) {
  registeredHandlers[trigger] = handler;
}

void SHI::MulticastHandler::handleUDPPacket(AsyncUDPPacket &packet) {
  const char *data = (const char *)(packet.data());
  if (packet.length() < 10) {
    auto handler = registeredHandlers[String(data)];
    if (handler != NULL) {
      handler(packet);
    }
  }
}
