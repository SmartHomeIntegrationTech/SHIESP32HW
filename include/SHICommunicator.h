#pragma once
#include "SHISensor.h"

namespace SHI {

class SHICommunicator {
  public:
    virtual void wifiConnected(){isConnected=true;};
    virtual void wifiDisconnected(){isConnected=false;};
    virtual void setupCommunication()=0;
    virtual void loopCommunication()=0;
    virtual void newReading(SHI::SensorReadings &reading, SHI::Channel &channel){};
    virtual void newStatus(SHI::Channel &channel, String message, bool isFatal){};
    virtual void newHardwareStatus(String message){};
  protected:
    SHICommunicator(String name) : name(name) {};
    String name;
    bool isConnected=false;
};

}
