#pragma once
#include <Arduino.h>
#include <memory>
#include "SHIVisitor.h"

namespace SHI {

class SHIObject {
  public:
    SHIObject(String name) : name(name) {}
    virtual String getName() {return name;} ;
    virtual void accept(SHI::Visitor &visitor) = 0;
  protected:
    String name;
};

} // namespace SHI

