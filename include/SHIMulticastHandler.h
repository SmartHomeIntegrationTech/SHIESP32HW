#include "SHICommunicator.h"
#include <AsyncUDP.h>
#include <map>

namespace SHI {

class SHIMulticastHandler : public SHI::SHICommunicator {
public:
  SHIMulticastHandler() : SHICommunicator("Multicast") {
    AuPacketHandlerFunction updateFunc = std::bind(
        &SHIMulticastHandler::updateHandler, this, std::placeholders::_1);
    registeredHandlers.insert({"UPDATE", updateFunc});
    AuPacketHandlerFunction resetFunc = std::bind(
        &SHIMulticastHandler::resetHandler, this, std::placeholders::_1);
    registeredHandlers.insert({"RESET", resetFunc});
    AuPacketHandlerFunction reconfFunc = std::bind(
        &SHIMulticastHandler::reconfHandler, this, std::placeholders::_1);
    registeredHandlers.insert({"RECONF", reconfFunc});
    AuPacketHandlerFunction infoFunc = std::bind(
        &SHIMulticastHandler::infoHandler, this, std::placeholders::_1);
    registeredHandlers.insert({"INFO", infoFunc});
    AuPacketHandlerFunction versionFunc = std::bind(
        &SHIMulticastHandler::versionHandler, this, std::placeholders::_1);
    registeredHandlers.insert({"VERSION", versionFunc});
  }
  void wifiConnected(){};
  void wifiDisconnected(){};
  void setupCommunication();
  void loopCommunication();
  void newReading(SHI::SensorReadings &reading, SHI::Channel &channel){};
  void newStatus(SHI::Channel &channel, String message, bool isFatal){};
  void newHardwareStatus(String message){};
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

} // namespace SHI