#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Arduino_LPS22HB.h>
#include "Checksum.h"

#define TEMP_ID 0xE2

extern BLECharacteristic dataCharacteristic;

static bool tempInitialized = false;

void setupTemperature() {
  Serial.println("[TEMPERATURE MODULE] INITIALIZING TEMPERATURE MODULE...");
  if (!BARO.begin()) {
    Serial.println("[TEMPERATURE MODULE] ERROR: TEMPERATURE MODULE INITIALIZATION FAILED");
    return;
  }
  Serial.println("[TEMPERATURE MODULE] TEMPERATURE MODULE INITIALIZED SUCCESSFULLY");
  tempInitialized = true;
}

bool triggerTemperature() {
  if (!tempInitialized || !BLE.connected()) return false;

  BARO.readPressure();

  float tempF = BARO.readTemperature();
  int16_t value = (int16_t)round(tempF);

  uint8_t packet[4];
  packet[0] = TEMP_ID;
  packet[1] = value & 0xFF;
  packet[2] = (value >> 8) & 0xFF;
  packet[3] = computeChecksum(packet, 3);

  const uint8_t RETRIES = 3;
  for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
    if (dataCharacteristic.writeValue(packet, sizeof(packet))) {
      Serial.println("[TEMPERATURE MODULE] TEMPERATURE PACKET TRANSMITTED");
      return true;
    }
    if (attempt < RETRIES) {
      Serial.println("[TEMPERATURE MODULE] WARNING: PACKET TRANSMISSION FAILED, RETRYING");
      delay(10);
    }
  }

  Serial.println("[TEMPERATURE MODULE] ERROR: TEMPERATURE PACKET TRANSMISSION FAILED AFTER MULTIPLE ATTEMPTS");
  return false;
}