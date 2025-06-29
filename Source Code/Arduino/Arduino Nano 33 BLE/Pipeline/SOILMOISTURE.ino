#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Checksum.h"

#define SOIL_ID    0xD3
#define SOIL_PIN   A1

extern BLECharacteristic dataCharacteristic;

static bool soilInitialized = false;

void setupSoilMoisture() {
  Serial.println("[SOIL MOISTURE MODULE] INITIALIZING SOIL MOISTURE SENSOR...");
  soilInitialized = true;
  if (soilInitialized) {
    Serial.println("[SOIL MOISTURE MODULE] SOIL MOISTURE SENSOR INITIALIZED SUCCESSFULLY");
  } else {
    Serial.println("[SOIL MOISTURE MODULE] ERROR: SOIL MOISTURE SENSOR INITIALIZATION FAILED");
  }
}

bool triggerSoilMoisture() {
  if (!soilInitialized) return false;
  if (!BLE.connected()) return false;

  uint16_t level = analogRead(SOIL_PIN);

  uint8_t packet[4];
  packet[0] = SOIL_ID;
  packet[1] = level & 0xFF;
  packet[2] = (level >> 8) & 0xFF;
  packet[3] = computeChecksum(packet, 3);

  const uint8_t RETRIES = 3;
  for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
    if (dataCharacteristic.writeValue(packet, sizeof(packet))) {
      Serial.println("[SOIL MOISTURE MODULE] SOIL MOISTURE PACKET TRANSMITTED");
      return true;
    }
    if (attempt < RETRIES) {
      Serial.println("[SOIL MOISTURE MODULE] WARNING: TRANSMISSION FAILED, ATTEMPTING RETRY");
      delay(10);
    }
  }

  Serial.println("[SOIL MOISTURE MODULE] ERROR: FAILED TO TRANSMIT SOIL MOISTURE PACKET AFTER MULTIPLE ATTEMPTS");
  return false;
}