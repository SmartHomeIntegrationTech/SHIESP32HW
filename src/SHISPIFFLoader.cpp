/**
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHISPIFFLoader.h"

#include <ArduinoJson.h>
#include <SHIFactory.h>
#include <SPIFFS.h>

#include "SHIESP32HW.h"

using fs::FS;
static const char *name = "SHISPIFFLoader";

// Loads the configuration from a file
SHI::FactoryErrors bootstrapFromConfig(const FS &fs, const char *filename) {
  // Open file for reading
  if (!SPIFFS.begin()) {
    return SHI::FactoryErrors::FailureToLoadFile;
  }
  File file = SPIFFS.open(filename);
  auto factory = SHI::Factory::get();
  String content = file.readString();
  ets_printf("%s content %s\n", filename, content.c_str());
  auto result = factory->construct(content.c_str());
  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
  return factory->getError(result);
}

bool writeConfigFile(const FS &fs, const char *filename, String content) {
  if (!SPIFFS.begin()) {
    return false;
  }
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    SHI_LOGERROR("Failed to open file for writing");
    return false;
  }
  file.clearWriteError();
  file.print(content);
  if (file.getWriteError() != 0) {
    SHI_LOGERROR("Failed to write content to file");
    return false;
  }
  file.close();
}
