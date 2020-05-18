/**
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#pragma once

#include <FS.h>
#include <SHIFactory.h>

SHI::FactoryErrors bootstrapFromConfig(const FS &fs, const char *filename);
bool writeConfigFile(const FS &fs, const char *filename, String content);
