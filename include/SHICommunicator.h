#pragma once
#include "SHISensor.h"

namespace SHI {

class SHICommunicator {
  public:
    virtual void wifiConnected()=0;
    virtual void wifiDisconnected()=0;
    virtual void setupCommunication()=0;
    virtual void loopCommunication()=0;
    virtual void newReading(SHI::SensorReadings &reading, SHI::Channel &channel)=0;
    virtual void newStatus(SHI::Channel &channel, String message, bool isFatal)=0;
    virtual void newHardwareStatus(String message);
  protected:
    SHICommunicator(String name) : name(name) {};
    String name;
};

}
