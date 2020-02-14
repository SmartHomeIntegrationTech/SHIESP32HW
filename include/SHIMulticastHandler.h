/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <AsyncUDP.h>

#include <map>

#include "SHICommunicator.h"

namespace SHI {

class MulticastHandler : public SHI::Communicator {
 public:
  MulticastHandler() : Communicator("Multicast") {
    registeredHandlers.insert(
        {"UPDATE", [this](AsyncUDPPacket &packet) { updateHandler(packet); }});
    registeredHandlers.insert(
        {"RESET", [this](AsyncUDPPacket &packet) { resetHandler(packet); }});
    registeredHandlers.insert(
        {"RECONF", [this](AsyncUDPPacket &packet) { reconfHandler(packet); }});
    registeredHandlers.insert(
        {"INFO", [this](AsyncUDPPacket &packet) { infoHandler(packet); }});
    registeredHandlers.insert({"VERSION", [this](AsyncUDPPacket &packet) {
                                 versionHandler(packet);
                               }});
  }
  void setupCommunication() override;
  void loopCommunication() override;
  void newReading(const SHI::SensorReadings &reading,
                  SHI::Sensor &sensor) override {}
  void newStatus(SHI::Sensor &sensor, String message, bool isFatal) override {}
  void newHardwareStatus(String message) override{};
  void accept(SHI::Visitor &visitor) override { visitor.visit(this); }
  void addUDPPacketHandler(String trigger, AuPacketHandlerFunction handler);

 private:
  void updateHandler(AsyncUDPPacket &packet);
  void resetHandler(AsyncUDPPacket &packet);
  void reconfHandler(AsyncUDPPacket &packet);
  void infoHandler(AsyncUDPPacket &packet);
  void versionHandler(AsyncUDPPacket &packet);
  void handleUDPPacket(AsyncUDPPacket &packet);
  void updateProgress(size_t a, size_t b);
  bool isUpdateAvailable();
  void startUpdate();
  bool doUpdate = false;
  AsyncUDP udpMulticast;
  std::map<String, AuPacketHandlerFunction> registeredHandlers;
};

}  // namespace SHI
