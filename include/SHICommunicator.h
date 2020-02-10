#pragma once
#include "SHISensor.h"
#include "SHIObject.h"

namespace SHI {

class Communicator : public SHI::SHIObject{
  public:
    virtual void wifiConnected(){isConnected=true;};
    virtual void wifiDisconnected(){isConnected=false;};
    virtual void setupCommunication()=0;
    virtual void loopCommunication()=0;
    virtual void newReading(SHI::SensorReadings &reading, SHI::Sensor &sensor){};
    virtual void newStatus(SHI::Sensor &sensor, String message, bool isFatal){};
    virtual void newHardwareStatus(String message){};
  protected:
    Communicator(String name) : SHIObject(name) {};
    bool isConnected=false;
};

}
