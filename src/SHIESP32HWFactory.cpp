/**
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include "SHIESP32HWFactory.h"

#include <SHIFactory.h>

#include "SHIESP32HW.h"

bool SHIESP32HWFactory::registerSHIESP32HWToFactory() {
  ets_printf("Registering SHIESP32HW");
  auto factory = SHI::Factory::get();
  factory->registerFactory("hw", [factory](const JsonObject &obj) {
    SHI::ESP32HWConfig config(obj);
    auto hw = new SHI::ESP32HW(config);
    return factory->defaultHardwareFactory(hw, obj);
  });
  ets_printf(" done\n");
  return true;
}

SHIESP32HWFactory::SHIESP32HWFactory() { registerSHIESP32HWToFactory(); }

#ifndef NO_AUTO_REGISTER
SHIESP32HWFactory shiESP32HWFactory;
#endif
