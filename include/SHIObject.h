#pragma once
#include <Arduino.h>
#include <memory>

namespace SHI {

class SHIObject {
  public:
    SHIObject(String name) : name(name) {}
    virtual String getName() {return name;} ;
  protected:
    String name;
};

} // namespace SHI

