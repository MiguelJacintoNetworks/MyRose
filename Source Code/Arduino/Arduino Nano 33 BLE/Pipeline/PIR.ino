#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Arduino_APDS9960.h>
#include "Checksum.h"

#define PIR_ID  0xC1

extern BLECharacteristic dataCharacteristic;

static bool pirInitialized = false;

void setupPIR() {
  Serial.println("[PIR MODULE] INITIALIZING PIR MODULE...");
  if (!APDS.begin()) {
    Serial.println("[PIR MODULE] ERROR: PIR MODULE INITIALIZATION FAILED");
    return;
  }
  Serial.println("[PIR MODULE] PIR MODULE INITIALIZED SUCCESSFULLY");
  pirInitialized = true;
}

bool triggerPIR() {
  if (!pirInitialized || !BLE.connected()) return false;
  int proximity = 0;
  if (APDS.proximityAvailable()) {
    proximity = APDS.readProximity();
    if (proximity < 0) {
      Serial.println("[PIR MODULE] ERROR: PIR READ FAILED");
      return false;
    }
  }

  uint8_t level = (uint8_t)proximity;

  uint8_t packet[3];
  packet[0] = PIR_ID;
  packet[1] = level;
  packet[2] = computeChecksum(packet, 2);

  const uint8_t RETRIES = 3;
  for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
    if (dataCharacteristic.writeValue(packet, sizeof(packet))) {
      Serial.println("[PIR MODULE] PIR PACKET TRANSMITTED");
      return true;
    }
    if (attempt < RETRIES) {
      Serial.println("[PIR MODULE] WARNING: PACKET TRANSMISSION FAILED, RETRYING");
      delay(10);
    }
  }

  Serial.println("[PIR MODULE] ERROR: PIR PACKET TRANSMISSION FAILED AFTER MULTIPLE ATTEMPTS");
  return false;
}
