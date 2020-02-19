/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include <Arduino.h>
#include <BLEBeacon.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <sys/time.h>

#include <string>

BLEAdvertising* pAdvertising;  // BLE Advertisement type

// Apple iBeacon E2C56DB5-dFFB-48D2-B060-D0F5A71096E0

// uint8_t PROXIMITY_UUID[] = {0xE2, 0xC5, 0x6D, 0xB5, 0xdF, 0xFB, 0x48, 0xD2,
//                          0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0};

// Randomly generated UUID for SHI B5C9D891-D4F7-409A-B871-2B941771AF44
uint8_t PROXIMITY_HOME_UUID[] = {0xb5, 0xc9, 0xd8, 0x91, 0xd4, 0xf7,
                                 0x40, 0x9a, 0xb8, 0x71, 0x2b, 0x94,
                                 0x17, 0x71, 0xaf, 0x44};

void setupBeacon() {
  BLEDevice::init("SHI_ESP32");

  // Create the BLE Server is no longer required to crete beacon
  // BLEServer *pServer = BLEDevice::createServer();

  pAdvertising = BLEDevice::getAdvertising();
  BLEBeacon oBeacon = BLEBeacon();
  oBeacon.setManufacturerId(
      0x4C00);  // fake Apple 0x004C LSB (ENDIAN_CHANGE_U16!)
  oBeacon.setProximityUUID(BLEUUID(PROXIMITY_HOME_UUID, 16, false));
  oBeacon.setMajor((0 & 0xFFFF0000) >> 16);
  oBeacon.setMinor(0 & 0xFFFF);
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();

  oAdvertisementData.setFlags(0x04);  // BR_EDR_NOT_SUPPORTED 0x04

  std::string strServiceData = "";

  strServiceData += static_cast<char>(26);    // Len
  strServiceData += static_cast<char>(0xFF);  // Type
  strServiceData += oBeacon.getData();
  oAdvertisementData.addData(strServiceData);

  std::string strResponseData = "";
  const char* name = "Home";
  strResponseData += static_cast<char>(strlen(name));
  strResponseData += static_cast<char>(0x9);  // Type Name
  strResponseData += name;

  strResponseData += static_cast<char>(2);
  strResponseData += static_cast<char>(0xa);  // Type Power
  strResponseData += static_cast<char>(0xf4);

  strResponseData += static_cast<char>(0xa);
  strResponseData += static_cast<char>(0x16);  // Type Service Data
  strResponseData += static_cast<char>(0x0d);  // Service UUID
  strResponseData += static_cast<char>(0xd0);  // Service UUID
  strResponseData += static_cast<char>(0x01);  // Beacon ID 1
  strResponseData += static_cast<char>(0x02);  // Beacon ID 2
  strResponseData += static_cast<char>(0x03);  // Beacon ID 3
  strResponseData += static_cast<char>(0x04);  // Beacon ID 4
  strResponseData += static_cast<char>(0x01);  // Firmware version 1
  strResponseData += static_cast<char>(0x00);  // Firmware version 2
  strResponseData += static_cast<char>(0x04);  // Battery level

  oScanResponseData.addData(strResponseData);

  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setScanResponseData(oScanResponseData);

  // Start advertising
  pAdvertising->start();
}
