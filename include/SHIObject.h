#pragma once
#include "SHIVisitor.h"
#include <Arduino.h>
#include <memory>
#include <utility>
#include <vector>

namespace SHI {

class SHIObject {
 public:
  explicit SHIObject(String name) : name(name) {}
  virtual String getName() { return name; }
  virtual void accept(SHI::Visitor &visitor) = 0;
  virtual std::vector<std::pair<String, String>> getStatistics() { return {}; }

 protected:
  String name;
};

}  // namespace SHI
