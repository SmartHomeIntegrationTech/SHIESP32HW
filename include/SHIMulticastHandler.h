#include "SHICommunicator.h"
#include <AsyncUDP.h>
#include <map>

namespace SHI {

class SHIMulticastHandler : public SHI::SHICommunicator {
public:
  SHIMulticastHandler() : SHICommunicator("Multicast") {
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