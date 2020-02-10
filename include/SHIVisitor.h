#pragma once

namespace SHI {

class Sensor;
class Channel;
class Hardware;
class Communicator;
class SensorData;

class Visitor {
  public:
    virtual void visit(SHI::Sensor *sensor) {};
    virtual void visit(SHI::Channel *channel) {};
    virtual void visit(SHI::Hardware *harwdware) {};
    virtual void visit(SHI::Communicator *communicator) {};
    virtual void visit(SHI::SensorData *data) {};
};

}